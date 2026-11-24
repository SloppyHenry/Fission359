// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pulling the region of a sketch that contains a point straight out of the sketch plane
// (AP-2.3a, "Ziehen"): the face of the region is placed on the plane `frame` and swept along the
// plane's normal; the two caps and every side are named after the curves of the region. The sweep
// may lean inwards or outwards (a taper) or go far enough to pass completely through a body
// ("through all").

#include <algorithm>
#include <cmath>
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
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <LocOpe_DPrism.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
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

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "extrude: a body is empty or was not created by this kernel");
    }
    return data->shape();
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

NamedSolid extrudeRegion(const std::vector<SketchCurve>& curves, Point2d inside,
                         const SketchFrame& frame, const ExtrudeSpec& spec) {
    if (!std::isfinite(spec.distance) ||
        (spec.extent != ExtrudeExtent::throughAll && std::abs(spec.distance) < 1e-9)) {
        invalidInput("the distance is zero or not a number");
    }
    if (!std::isfinite(spec.taperDegrees) || std::abs(spec.taperDegrees) >= 89.0) {
        invalidInput("the taper must be less than 89 degrees");
    }
    if (spec.taperDegrees < 0.0) {
        // LocOpe_DPrism rounds the corners of an outward taper; sharp corners come later.
        invalidInput("an outward taper is not supported yet");
    }
    if (spec.taperDegrees != 0.0 && spec.extent == ExtrudeExtent::symmetric) {
        invalidInput("a taper needs a pull to one side");
    }
    if (spec.extent == ExtrudeExtent::throughAll && !spec.through) {
        invalidInput("through all needs a body");
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
        double pull =
            spec.extent == ExtrudeExtent::symmetric ? std::abs(spec.distance) : spec.distance;
        if (spec.extent == ExtrudeExtent::throughAll) {
            // Far enough to pass completely through `through`: the farthest corner of its
            // bounding box on the side the sign of `distance` gives, plus a margin of 1 mm.
            const int side = spec.distance < 0.0 ? -1 : 1;
            Bnd_Box box;
            BRepBndLib::Add(shapeOf(*spec.through), box);
            if (box.IsVoid()) {
                invalidInput("the body is empty");
            }
            const gp_Pnt lo = box.CornerMin();
            const gp_Pnt hi = box.CornerMax();
            double farthest = -std::numeric_limits<double>::infinity();
            for (int i = 0; i < 8; ++i) {
                const gp_Pnt corner((i & 1) != 0 ? hi.X() : lo.X(), (i & 2) != 0 ? hi.Y() : lo.Y(),
                                    (i & 4) != 0 ? hi.Z() : lo.Z());
                const Point3d fromOrigin{corner.X() - frame.origin.x, corner.Y() - frame.origin.y,
                                         corner.Z() - frame.origin.z};
                farthest = std::max(farthest, dot(fromOrigin, n) * side);
            }
            if (farthest <= 0.0) {
                invalidInput("the body is not on that side");
            }
            pull = side * (farthest + 1.0);
        }
        TopoDS_Shape result;
        std::vector<std::pair<TopoDS_Shape, std::string>> roles;
        std::map<std::size_t, int> seen;
        if (spec.taperDegrees == 0.0) {
            BRepPrimAPI_MakePrism prism(placed, gp_Vec(n.x * pull, n.y * pull, n.z * pull));
            prism.Build();
            if (!prism.IsDone()) {
                kernelFailed("the sweep did not work");
            }
            result = prism.Shape();
            roles.push_back({prism.FirstShape(), "capStart"});
            roles.push_back({prism.LastShape(), "capEnd"});
            for (const auto& [edge, indices] : region->edgeCurves) {
                const TopoDS_Shape placedEdge = moved.ModifiedShape(edge);
                for (const TopoDS_Shape& side : prism.Generated(placedEdge)) {
                    roles.push_back({side, sideRole(indices, seen)});
                }
            }
        } else {
            // A taper: the sides lean by the angle against the pull, so a positive taper makes
            // the far cap smaller. OCCT's draft angle counts from the pull direction the same
            // way; should the kernel's sign turn out the opposite, negate it here — the golden
            // tests with +5° and -5° decide.
            const double angleRadians = spec.taperDegrees * std::numbers::pi / 180.0;
            // LocOpe_DPrism measures the height along the leaning side, not along the normal.
            LocOpe_DPrism dprism(placed, pull / std::cos(angleRadians), angleRadians);
            if (!dprism.IsDone()) {
                kernelFailed("the sweep did not work");
            }
            result = dprism.Shape();
            roles.push_back({dprism.FirstShape(), "capStart"});
            roles.push_back({dprism.LastShape(), "capEnd"});
            for (const auto& [edge, indices] : region->edgeCurves) {
                const TopoDS_Shape placedEdge = moved.ModifiedShape(edge);
                for (const TopoDS_Shape& side : dprism.Shapes(placedEdge)) {
                    roles.push_back({side, sideRole(indices, seen)});
                }
            }
        }
        NamedSolid named =
            nameFaces(result, "extrude", [&](const TopoDS_Face& face) -> std::string {
                // A cap of a tapered pull comes as a shell: look at the faces of each shape.
                for (const auto& [shape, role] : roles) {
                    for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next()) {
                        if (e.Current().IsSame(face)) {
                            return role;
                        }
                    }
                }
                return {};
            });
        if (!BRepCheck_Analyzer(result).IsValid()) {
            kernelFailed("the result is not valid");
        }
        return named;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "extrude: the kernel failed");
    }
}

} // namespace fission::geo::occt
