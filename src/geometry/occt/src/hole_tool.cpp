// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The cutting tool of a drilled hole (AP-2.7b): a body standing on the origin, axis along +Z,
// reaching from `above` the face down to -depth, to be subtracted from the material.

#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"

#include "occt_build.h"
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kTol = 1e-6;

[[noreturn]] void refuse(const char* why) {
    throw GeometryError(GeometryErrorCode::invalidInput, std::string("hole: ") + why);
}

// Sizes that do not fit are refused before anything is built.
void requireFitting(const HoleToolSpec& spec) {
    const double values[] = {spec.diameter,  spec.depth,    spec.above,   spec.headDiameter,
                             spec.headDepth, spec.nutWidth, spec.nutDepth};
    for (const double value : values) {
        if (!std::isfinite(value)) {
            refuse("sizes must be numbers");
        }
    }
    if (spec.diameter <= 0.0 || spec.depth <= 0.0 || spec.above < 0.0) {
        refuse("diameter and depth must be positive, above must not be negative");
    }
    switch (spec.kind) {
    case HoleKind::plain:
        break;
    case HoleKind::counterbore:
        if (!(spec.headDiameter > spec.diameter) || !(spec.headDepth > 0.0) ||
            !(spec.headDepth < spec.depth)) {
            refuse("the counterbore must be wider than the hole and shallower than the depth");
        }
        break;
    case HoleKind::countersink:
        if (!(spec.headDiameter > spec.diameter) ||
            !((spec.headDiameter - spec.diameter) / 2.0 < spec.depth)) {
            refuse("the countersink must be wider than the hole and fit into the depth");
        }
        break;
    case HoleKind::nutTrap:
        if (!(spec.nutWidth > spec.diameter) || !(spec.nutDepth > 0.0) ||
            !(spec.nutDepth < spec.depth)) {
            refuse("the nut trap must be wider than the hole and shallower than the depth");
        }
        break;
    }
}

// The closed polygon of the hole's cross-section in the XZ plane (x = radius >= 0, z), going
// round from the axis above the face back to the axis below.
std::vector<std::pair<double, double>> profileOf(const HoleToolSpec& spec, double r, double R) {
    const double above = spec.above;
    const double depth = spec.depth;
    switch (spec.kind) {
    case HoleKind::plain:
        return {{0.0, above}, {r, above}, {r, -depth}, {0.0, -depth}};
    case HoleKind::counterbore:
        return {{0.0, above},         {R, above},  {R, -spec.headDepth},
                {r, -spec.headDepth}, {r, -depth}, {0.0, -depth}};
    case HoleKind::countersink: {
        const double h = R - r; // 90° included angle: the radius grows by h over the height h
        return {{0.0, above}, {R, above}, {R, 0.0}, {r, -h}, {r, -depth}, {0.0, -depth}};
    }
    case HoleKind::nutTrap:
        break; // built as a prism, not revolved
    }
    return {};
}

// The polygon revolved 360° about the Z axis. The edges on the axis (x = 0) degenerate in the
// revolution; that is normal for MakeRevol.
TopoDS_Shape revolvedTool(const std::vector<std::pair<double, double>>& outline) {
    BRepBuilderAPI_MakePolygon polygon;
    for (const auto& [x, z] : outline) {
        polygon.Add(gp_Pnt(x, 0.0, z));
    }
    polygon.Close();
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(polygon.Wire(), /*OnlyPlane=*/true).Face();
    const gp_Ax1 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    BRepPrimAPI_MakeRevol revol(face, axis);
    if (!revol.IsDone()) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "hole: the kernel failed");
    }
    return revol.Shape();
}

// Hexagon prism of width across flats `nutWidth` from z = -nutDepth up to z = above, fused with
// the hole below it (the cylinder reaches past the prism's floor, so the fuse overlaps).
TopoDS_Shape nutTrapTool(const HoleToolSpec& spec, double r) {
    const double corner = spec.nutWidth / std::sqrt(3.0); // radius of the hexagon's corners
    BRepBuilderAPI_MakePolygon hexagon;
    for (int i = 0; i < 6; ++i) {
        const double angle = 60.0 * std::numbers::pi / 180.0 * static_cast<double>(i);
        hexagon.Add(gp_Pnt(corner * std::cos(angle), corner * std::sin(angle), -spec.nutDepth));
    }
    hexagon.Close();
    const TopoDS_Face hexFace = BRepBuilderAPI_MakeFace(hexagon.Wire(), /*OnlyPlane=*/true).Face();
    const TopoDS_Shape prism =
        BRepPrimAPI_MakePrism(hexFace, gp_Vec(0.0, 0.0, spec.above + spec.nutDepth)).Shape();
    const gp_Ax2 axis(gp_Pnt(0.0, 0.0, -spec.depth), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(axis, r, spec.depth - spec.nutDepth / 2.0).Shape();
    BRepAlgoAPI_Fuse fuse(prism, cylinder);
    if (!fuse.IsDone() || fuse.HasErrors()) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "hole: the kernel failed");
    }
    ShapeUpgrade_UnifySameDomain unify(fuse.Shape(), /*UnifyEdges=*/true, /*UnifyFaces=*/true,
                                       /*ConcatBSplines=*/false);
    unify.Build();
    return unify.Shape();
}

// The role of one face of the tool. An empty role is a bug of the builder (nameFaces refuses it).
std::string roleOf(const TopoDS_Face& face, const HoleToolSpec& spec, double r, double R) {
    const BRepAdaptor_Surface surface(face);
    const GeomAbs_SurfaceType type = surface.GetType();
    if (type == GeomAbs_Plane) {
        const gp_Pln plane = surface.Plane();
        const gp_Dir normal = plane.Axis().Direction();
        if (std::abs(std::abs(normal.Z()) - 1.0) <= kTol) {
            const double z = faceCentre(face).Z();
            if (std::abs(z - spec.above) <= kTol) {
                return "top";
            }
            if (std::abs(z + spec.depth) <= kTol) {
                return "bottom";
            }
            if (spec.kind == HoleKind::counterbore && std::abs(z + spec.headDepth) <= kTol) {
                return "headFloor";
            }
            if (spec.kind == HoleKind::nutTrap && std::abs(z + spec.nutDepth) <= kTol) {
                return "nutFloor";
            }
            return "";
        }
        // A side of the nut trap, numbered by the angle of the face's middle. The middles sit at
        // 30, 90, ... degrees; the +30 keeps them off the half-way points of the rounding.
        const gp_Pnt centre = faceCentre(face);
        double angle = std::atan2(centre.Y(), centre.X()) * 180.0 / std::numbers::pi;
        if (angle < 0.0) {
            angle += 360.0;
        }
        const int index = 1 + static_cast<int>(std::round((angle + 30.0) / 60.0)) % 6;
        return "nutWall[" + std::to_string(index) + "]";
    }
    if (type == GeomAbs_Cylinder) {
        const gp_Cylinder cylinder = surface.Cylinder();
        const double radius = cylinder.Radius();
        if (std::abs(radius - r) <= kTol) {
            return "wall";
        }
        if (std::abs(radius - R) <= kTol) {
            return "headWall";
        }
        return "";
    }
    if (type == GeomAbs_Cone) {
        return "countersink";
    }
    return "";
}

} // namespace

NamedSolid makeHoleToolNamed(const HoleToolSpec& spec) {
    try {
        requireFitting(spec);
        const double r = spec.diameter / 2.0;
        const double R = spec.headDiameter / 2.0;
        const TopoDS_Shape shape = spec.kind == HoleKind::nutTrap
                                       ? nutTrapTool(spec, r)
                                       : revolvedTool(profileOf(spec, r, R));
        if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid()) {
            throw GeometryError(GeometryErrorCode::kernelFailure,
                                "hole: the kernel returned an invalid body");
        }
        return nameFaces(shape, "hole",
                         [&](const TopoDS_Face& face) { return roleOf(face, spec, r, R); });
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "hole: the kernel failed");
    }
}

} // namespace fission::geo::occt
