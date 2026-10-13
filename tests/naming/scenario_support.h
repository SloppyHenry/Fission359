// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Shared helpers for the AP-0.6 scenarios: the intended target of a reference is described
// from the model parameters and found with geometric queries (truth.h), never with names.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "features.h"
#include "report.h"
#include "truth.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

namespace fission::test::naming {

enum class Side { negX, posX, negY, posY, negZ, posZ };

struct BoxDims {
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
};

inline gp_Dir sideNormal(Side side) {
    switch (side) {
    case Side::negX:
        return gp_Dir(-1, 0, 0);
    case Side::posX:
        return gp_Dir(1, 0, 0);
    case Side::negY:
        return gp_Dir(0, -1, 0);
    case Side::posY:
        return gp_Dir(0, 1, 0);
    case Side::negZ:
        return gp_Dir(0, 0, -1);
    case Side::posZ:
        return gp_Dir(0, 0, 1);
    }
    return gp_Dir(0, 0, 1);
}

// A point on the plane of that side of a box standing at the origin.
inline gp_Pnt sidePoint(const BoxDims& d, Side side) {
    switch (side) {
    case Side::posX:
        return gp_Pnt(d.dx, 0, 0);
    case Side::posY:
        return gp_Pnt(0, d.dy, 0);
    case Side::posZ:
        return gp_Pnt(0, 0, d.dz);
    default:
        return gp_Pnt(0, 0, 0);
    }
}

inline truth::Shapes boxFace(const TopoDS_Shape& shape, const BoxDims& d, Side side) {
    return truth::planarFaces(shape, sidePoint(d, side), sideNormal(side));
}

inline truth::Shapes boxEdge(const TopoDS_Shape& shape, const BoxDims& d, Side a, Side b) {
    return truth::straightEdgesBetween(shape, sidePoint(d, a), sideNormal(a), sidePoint(d, b),
                                       sideNormal(b));
}

inline double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

// ---- sketch outlines (counter-clockwise polygons; segment i runs from point i to point i+1) ----

inline std::vector<SketchPoint> rectangleSketch(double dx, double dy, int firstSegment = 0) {
    return {{0, 0, firstSegment},
            {dx, 0, firstSegment + 1},
            {dx, dy, firstSegment + 2},
            {0, dy, firstSegment + 3}};
}

// The side face of the extrusion that stands on the sketch segment with the given id.
inline truth::Shapes segmentFace(const TopoDS_Shape& shape, const std::vector<SketchPoint>& sketch,
                                 int segment) {
    for (std::size_t i = 0; i < sketch.size(); ++i) {
        if (sketch[i].segment != segment) {
            continue;
        }
        const SketchPoint& a = sketch[i];
        const SketchPoint& b = sketch[(i + 1) % sketch.size()];
        const gp_Dir outward(b.y - a.y, -(b.x - a.x), 0.0); // right of the direction (CCW)
        return truth::planarFaces(shape, gp_Pnt(a.x, a.y, 0.0), outward);
    }
    return {};
}

// The straight edge where the extrusion's top cap meets the side face of a segment.
inline truth::Shapes segmentTopEdge(const TopoDS_Shape& shape,
                                    const std::vector<SketchPoint>& sketch, int segment,
                                    double height) {
    for (std::size_t i = 0; i < sketch.size(); ++i) {
        if (sketch[i].segment != segment) {
            continue;
        }
        const SketchPoint& a = sketch[i];
        const SketchPoint& b = sketch[(i + 1) % sketch.size()];
        const gp_Dir outward(b.y - a.y, -(b.x - a.x), 0.0);
        return truth::straightEdgesBetween(shape, gp_Pnt(a.x, a.y, 0.0), outward,
                                           gp_Pnt(0, 0, height), gp_Dir(0, 0, 1));
    }
    return {};
}

// New point behind point `index`; the segment that starts there gets the id `segment`.
inline void insertPoint(std::vector<SketchPoint>& sketch, std::size_t index, double x, double y,
                        int segment) {
    sketch.insert(sketch.begin() + static_cast<std::ptrdiff_t>(index) + 1, {x, y, segment});
}

// Removes point `index`: the previous segment now runs to the next point, this one is gone.
inline void removePoint(std::vector<SketchPoint>& sketch, std::size_t index) {
    sketch.erase(sketch.begin() + static_cast<std::ptrdiff_t>(index));
}

// Exactly one element: the pick of the user.
inline TopoDS_Shape only(const truth::Shapes& shapes) {
    REQUIRE(shapes.size() == 1);
    return shapes.front();
}

// Resolves with both strictness levels and counts both; returns the strict verdict.
inline Verdict measure(const std::string& group, const Body& body, const Reference& ref,
                       const truth::Shapes& truthSet) {
    const Verdict strict =
        record(group, Strictness::strict, resolve(body, ref, Strictness::strict), truthSet);
    record(group, Strictness::lenient, resolve(body, ref, Strictness::lenient), truthSet);
    return strict;
}

} // namespace fission::test::naming
