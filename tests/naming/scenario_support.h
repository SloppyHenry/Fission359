// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Shared helpers for the AP-0.6 scenarios: the intended target of a reference is described
// from the model parameters and found with geometric queries (truth.h), never with names.

#pragma once

#include <catch2/catch_test_macros.hpp>

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
