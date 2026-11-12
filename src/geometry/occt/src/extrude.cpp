// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pulling the region of a sketch that contains a point straight out of the sketch plane
// (AP-2.3a, "Ziehen"): the face of the region is placed on the plane `frame` and swept along the
// plane's normal; the two caps and every side are named after the curves of the region.

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/sketch_regions.h"

#include "occt_build.h"
#include "sketch_region_face.h"
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

[[noreturn]] void invalidInput(const char* what) {
    throw GeometryError(GeometryErrorCode::invalidInput, std::string("extrude: ") + what,
                        "extrude");
}

[[noreturn]] void kernelFailed(const char* what) {
    throw GeometryError(GeometryErrorCode::kernelFailure, std::string("extrude: ") + what);
}

double lengthOf(const Point3d& v) {
    return std::hypot(v.x, v.y, v.z);
}

double dot(const Point3d& a, const Point3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Point3d cross(const Point3d& a, const Point3d& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

} // namespace

NamedSolid extrudeRegion(const std::vector<SketchCurve>& curves, Point2d inside,
                         const SketchFrame& frame, const ExtrudeSpec& spec) {
    if (!std::isfinite(spec.distance) || std::abs(spec.distance) < 1e-9) {
        invalidInput("the distance is zero or not a number");
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
        const Point3d n = cross(frame.u, frame.v); // the normal of the sketch plane
        // gp_Trsf::SetTransformation(from, to) turns coordinates given in `to` into coordinates
        // in `from`, so the sketch plane (XY, u = X, v = Y) is placed on `frame` by asking for
        // the move from `frame` to the world: SetTransformation(target, gp_Ax3()). (Easy to get
        // backwards; the test with a turned frame checks the direction.)
        const gp_Ax3 target(gp_Pnt(frame.origin.x, frame.origin.y, frame.origin.z),
                            gp_Dir(n.x, n.y, n.z), gp_Dir(frame.u.x, frame.u.y, frame.u.z));
        gp_Trsf place;
        place.SetTransformation(target, gp_Ax3());
        if (spec.extent == ExtrudeExtent::symmetric) {
            // half of the total distance to the other side of the sketch plane
            gp_Trsf shift;
            shift.SetTranslation(gp_Vec(-n.x * std::abs(spec.distance) / 2.0,
                                        -n.y * std::abs(spec.distance) / 2.0,
                                        -n.z * std::abs(spec.distance) / 2.0));
            place = shift * place; // first onto the plane, then along its normal
        }
        BRepBuilderAPI_Transform moved(region->face, place, /*Copy=*/true);
        const TopoDS_Face placed = TopoDS::Face(moved.Shape());
        const double pull =
            spec.extent == ExtrudeExtent::symmetric ? std::abs(spec.distance) : spec.distance;
        BRepPrimAPI_MakePrism prism(placed, gp_Vec(n.x * pull, n.y * pull, n.z * pull));
        prism.Build();
        if (!prism.IsDone()) {
            kernelFailed("the sweep did not work");
        }
        std::vector<std::pair<TopoDS_Shape, std::string>> roles;
        roles.push_back({prism.FirstShape(), "capStart"});
        roles.push_back({prism.LastShape(), "capEnd"});
        std::map<std::size_t, int> seen;
        for (const auto& [edge, indices] : region->edgeCurves) {
            const TopoDS_Shape placedEdge = moved.ModifiedShape(edge);
            const std::size_t c = *std::ranges::min_element(indices);
            const int k = ++seen[c];
            std::string role = "side[c" + std::to_string(c) + "]";
            if (k > 1) {
                role = "side[c" + std::to_string(c) + ":" + std::to_string(k) + "]";
            }
            for (const TopoDS_Shape& side : prism.Generated(placedEdge)) {
                roles.push_back({side, role});
            }
        }
        NamedSolid named =
            nameFaces(prism.Shape(), "extrude", [&](const TopoDS_Face& face) -> std::string {
                for (const auto& [shape, role] : roles) {
                    if (shape.IsSame(face)) {
                        return role;
                    }
                }
                return {};
            });
        if (!BRepCheck_Analyzer(prism.Shape()).IsValid()) {
            kernelFailed("the result is not valid");
        }
        return named;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "extrude: the kernel failed");
    }
}

} // namespace fission::geo::occt
