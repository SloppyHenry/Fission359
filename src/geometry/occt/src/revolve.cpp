// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Turning the region of a sketch about an axis that lies in the sketch plane (AP-2.3c,
// "Drehkörper"): the face of the region is placed on the plane `frame` and revolved about the
// axis; every side is named after the curve of the region it comes from, and a turn of less
// than 360 degrees gets two caps. The region must lie on one side of the axis (touching it is
// allowed).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/sketch_regions.h"

#include "occt_build.h"
#include "sketch_region_face.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace fission::geo::occt {

namespace {

[[noreturn]] void invalidInput(const char* what) {
    throw GeometryError(GeometryErrorCode::invalidInput, std::string("revolve: ") + what,
                        "revolve");
}

[[noreturn]] void kernelFailed(const char* what) {
    throw GeometryError(GeometryErrorCode::kernelFailure, std::string("revolve: ") + what);
}

// How near the area may come to the axis (mm): touching it is allowed.
constexpr double kAxisTolerance = 1e-7;

double lengthOf(const Point3d& v) {
    return std::hypot(v.x, v.y, v.z);
}

double dot(const Point3d& a, const Point3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Point3d cross(const Point3d& a, const Point3d& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// The role of the sides that one curve of the region gives: "side[c<i>]", the second and later
// ":<k>" (k = 2, 3, ...).
std::string sideRole(const std::vector<std::size_t>& indices, std::map<std::size_t, int>& seen) {
    const std::size_t c = *std::ranges::min_element(indices);
    const int k = ++seen[c];
    std::string role = "side[c" + std::to_string(c) + "]";
    if (k > 1) {
        role = "side[c" + std::to_string(c) + ":" + std::to_string(k) + "]";
    }
    return role;
}

} // namespace

NamedSolid revolveRegion(const std::vector<SketchCurve>& curves, Point2d inside,
                         const SketchFrame& frame, const RevolveSpec& spec) {
    if (!std::isfinite(spec.axisPoint.x) || !std::isfinite(spec.axisPoint.y) ||
        !std::isfinite(spec.axisDirection.x) || !std::isfinite(spec.axisDirection.y) ||
        std::hypot(spec.axisDirection.x, spec.axisDirection.y) < 1e-12) {
        invalidInput("the axis is zero or not a number");
    }
    if (!std::isfinite(spec.angleDegrees) || spec.angleDegrees <= 0.0 ||
        spec.angleDegrees > 360.0) {
        invalidInput("the angle must be more than 0 and at most 360 degrees");
    }
    if (std::abs(lengthOf(frame.u) - 1.0) > 1e-9 || std::abs(lengthOf(frame.v) - 1.0) > 1e-9 ||
        std::abs(dot(frame.u, frame.v)) > 1e-9) {
        invalidInput("the frame is not two perpendicular unit vectors");
    }
    try {
        const std::optional<RegionFace> region = regionFaceAt(curves, inside);
        if (!region) {
            invalidInput("no region at that point");
        }
        // The signed distance of the region's boundary from the axis line, in sketch
        // coordinates: the area must stay on one side of it. Lines reach their extremes at
        // their ends; the rest of the boundary is sampled finely enough that a real crossing
        // (deeper than the tolerance) shows up.
        const double dirLength = std::hypot(spec.axisDirection.x, spec.axisDirection.y);
        const auto signedDistance = [&](const gp_Pnt& p) {
            return ((p.X() - spec.axisPoint.x) * spec.axisDirection.y -
                    (p.Y() - spec.axisPoint.y) * spec.axisDirection.x) /
                   dirLength;
        };
        double nearest = std::numeric_limits<double>::infinity();
        double farthest = -std::numeric_limits<double>::infinity();
        const auto note = [&](const gp_Pnt& p) {
            const double d = signedDistance(p);
            nearest = std::min(nearest, d);
            farthest = std::max(farthest, d);
        };
        for (TopExp_Explorer e(region->face, TopAbs_EDGE); e.More(); e.Next()) {
            const BRepAdaptor_Curve curve(TopoDS::Edge(e.Current()));
            const GCPnts_QuasiUniformDeflection points(curve, 1e-3);
            for (int i = 1; i <= points.NbPoints(); ++i) {
                note(points.Value(i));
            }
            for (TopExp_Explorer v(e.Current(), TopAbs_VERTEX); v.More(); v.Next()) {
                note(BRep_Tool::Pnt(TopoDS::Vertex(v.Current())));
            }
        }
        if (nearest < -kAxisTolerance && farthest > kAxisTolerance) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "revolve: the area crosses the axis", "axis");
        }
        const Point3d n = cross(frame.u, frame.v); // the normal of the sketch plane
        // gp_Trsf::SetTransformation(from, to) turns coordinates given in `to` into coordinates
        // in `from`, so the sketch plane (XY, u = X, v = Y) is placed on `frame` by asking for
        // the move from `frame` to the world: SetTransformation(target, gp_Ax3()). (Easy to get
        // backwards; the test with a turned frame checks the direction.)
        const gp_Ax3 target(gp_Pnt(frame.origin.x, frame.origin.y, frame.origin.z),
                            gp_Dir(n.x, n.y, n.z), gp_Dir(frame.u.x, frame.u.y, frame.u.z));
        gp_Trsf place;
        place.SetTransformation(target, gp_Ax3());
        BRepBuilderAPI_Transform moved(region->face, place, /*Copy=*/true);
        const TopoDS_Face placed = TopoDS::Face(moved.Shape());
        // The axis in world coordinates: a point of the sketch plane and a direction of it, both
        // moved by the frame (u*du + v*dv).
        const Point3d axisPoint{
            frame.origin.x + frame.u.x * spec.axisPoint.x + frame.v.x * spec.axisPoint.y,
            frame.origin.y + frame.u.y * spec.axisPoint.x + frame.v.y * spec.axisPoint.y,
            frame.origin.z + frame.u.z * spec.axisPoint.x + frame.v.z * spec.axisPoint.y};
        const Point3d axisDirection{
            frame.u.x * spec.axisDirection.x + frame.v.x * spec.axisDirection.y,
            frame.u.y * spec.axisDirection.x + frame.v.y * spec.axisDirection.y,
            frame.u.z * spec.axisDirection.x + frame.v.z * spec.axisDirection.y};
        const gp_Ax1 axis(gp_Pnt(axisPoint.x, axisPoint.y, axisPoint.z),
                          gp_Dir(axisDirection.x, axisDirection.y, axisDirection.z));
        // A full turn is passed as exactly 2*pi: an angle a hair below would get caps.
        const bool full = spec.angleDegrees == 360.0;
        const double angleRadians =
            full ? 2.0 * std::numbers::pi : spec.angleDegrees * std::numbers::pi / 180.0;
        BRepPrimAPI_MakeRevol revol(placed, axis, angleRadians);
        revol.Build();
        if (!revol.IsDone()) {
            kernelFailed("the turn did not work");
        }
        const TopoDS_Shape result = revol.Shape();
        std::vector<std::pair<TopoDS_Shape, std::string>> roles;
        std::map<std::size_t, int> seen;
        if (!full) {
            roles.push_back({revol.FirstShape(), "capStart"});
            roles.push_back({revol.LastShape(), "capEnd"});
        }
        for (const auto& [edge, indices] : region->edgeCurves) {
            const TopoDS_Shape placedEdge = moved.ModifiedShape(edge);
            // An edge on the axis gives a degenerate shape without a face: no side to name.
            for (const TopoDS_Shape& side : revol.Generated(placedEdge)) {
                roles.push_back({side, sideRole(indices, seen)});
            }
        }
        NamedSolid named =
            nameFaces(result, "revolve", [&](const TopoDS_Face& face) -> std::string {
                for (const auto& [shape, role] : roles) {
                    for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next()) {
                        if (e.Current().IsSame(face)) {
                            return role;
                        }
                    }
                }
                // A full turn gives no generated face for an edge that stands across the axis
                // (OCCT leaves the flat ring unlisted): the face that carries the edge where the
                // sweep started is the one that edge made.
                for (const auto& [edge, indices] : region->edgeCurves) {
                    const TopoDS_Edge placedEdge = TopoDS::Edge(moved.ModifiedShape(edge));
                    BRepAdaptor_Curve curve(placedEdge);
                    const gp_Pnt middle =
                        curve.Value(0.5 * (curve.FirstParameter() + curve.LastParameter()));
                    const TopoDS_Vertex probe = BRepBuilderAPI_MakeVertex(middle);
                    BRepExtrema_DistShapeShape distance(probe, face);
                    if (distance.IsDone() && distance.Value() < 1e-6) {
                        std::map<std::size_t, int> once;
                        return sideRole(indices, once);
                    }
                }
                return {};
            });
        if (!BRepCheck_Analyzer(result).IsValid()) {
            kernelFailed("the result is not valid");
        }
        return named;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "revolve: the kernel failed");
    }
}

} // namespace fission::geo::occt
