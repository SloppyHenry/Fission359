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
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/sketch_regions.h"

#include "occt_build.h"
#include "sketch_region_face.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <LocOpe_DPrism.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
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
        } else if (spec.taperDegrees > 0.0) {
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
        } else {
            // An outward taper with sharp corners: LocOpe_DPrism would round them, so loft
            // between the near profile and an offset copy of it instead. The far profile is
            // the near one grown by `grow` on each side and moved to the far plane.
            const double angleRadians = spec.taperDegrees * std::numbers::pi / 180.0;
            const double grow = std::abs(pull) * std::tan(std::abs(angleRadians));
            BRepOffsetAPI_MakeOffset offset(placed, GeomAbs_Intersection);
            offset.Perform(grow);
            if (!offset.IsDone()) {
                kernelFailed("the taper is too steep for this shape");
            }
            gp_Trsf farShift;
            farShift.SetTranslation(gp_Vec(n.x * pull, n.y * pull, n.z * pull));
            BRepBuilderAPI_Transform farMoved(offset.Shape(), farShift, /*Copy=*/true);
            std::vector<TopoDS_Wire> farWires;
            for (TopExp_Explorer e(farMoved.Shape(), TopAbs_WIRE); e.More(); e.Next()) {
                farWires.push_back(TopoDS::Wire(e.Current()));
            }
            const TopoDS_Wire outerNear = BRepTools::OuterWire(placed);
            std::vector<TopoDS_Wire> nearWires{outerNear};
            for (TopExp_Explorer e(placed, TopAbs_WIRE); e.More(); e.Next()) {
                const TopoDS_Wire wire = TopoDS::Wire(e.Current());
                if (!wire.IsSame(outerNear)) {
                    nearWires.push_back(wire);
                }
            }
            if (farWires.size() != nearWires.size()) {
                kernelFailed("the taper is too steep for this shape");
            }
            // The centre of a wire's bounding box, projected onto the sketch plane.
            const auto projectedCentre = [&](const TopoDS_Shape& wire) {
                Bnd_Box box;
                BRepBndLib::Add(wire, box);
                if (box.IsVoid()) {
                    kernelFailed("the taper is too steep for this shape");
                }
                const gp_Pnt lo = box.CornerMin();
                const gp_Pnt hi = box.CornerMax();
                const Point3d centre{(lo.X() + hi.X()) / 2.0, (lo.Y() + hi.Y()) / 2.0,
                                     (lo.Z() + hi.Z()) / 2.0};
                const double along = dot({centre.x - frame.origin.x, centre.y - frame.origin.y,
                                          centre.z - frame.origin.z},
                                         n);
                return Point3d{centre.x - n.x * along, centre.y - n.y * along,
                               centre.z - n.z * along};
            };
            // The diagonal of a wire's bounding box.
            const auto sizeOf = [](const TopoDS_Shape& wire) {
                Bnd_Box box;
                BRepBndLib::Add(wire, box);
                return box.IsVoid() ? 0.0 : box.CornerMin().Distance(box.CornerMax());
            };
            // Match each near wire to the unused far wire with the closest centre and size.
            std::vector<TopoDS_Wire> farMatched;
            std::vector<bool> taken(farWires.size(), false);
            for (const TopoDS_Wire& near : nearWires) {
                const Point3d nearCentre = projectedCentre(near);
                double best = std::numeric_limits<double>::infinity();
                int bestIndex = -1;
                for (std::size_t j = 0; j < farWires.size(); ++j) {
                    if (taken[j]) {
                        continue;
                    }
                    const Point3d farCentre = projectedCentre(farWires[j]);
                    // Wires round the same middle (a ring) differ in size: compare that too.
                    const double d =
                        lengthOf({nearCentre.x - farCentre.x, nearCentre.y - farCentre.y,
                                  nearCentre.z - farCentre.z}) +
                        std::abs(sizeOf(near) - sizeOf(farWires[j]));
                    if (d < best) {
                        best = d;
                        bestIndex = static_cast<int>(j);
                    }
                }
                if (bestIndex < 0) {
                    kernelFailed("the taper is too steep for this shape");
                }
                taken[static_cast<std::size_t>(bestIndex)] = true;
                farMatched.push_back(farWires[static_cast<std::size_t>(bestIndex)]);
            }
            // Loft each near wire to its far wire; the outer loft is the body, the hole lofts
            // are cut out of it.
            std::vector<std::unique_ptr<BRepOffsetAPI_ThruSections>> lofts;
            for (std::size_t i = 0; i < nearWires.size(); ++i) {
                lofts.push_back(std::make_unique<BRepOffsetAPI_ThruSections>(/*isSolid=*/true,
                                                                             /*ruled=*/true));
                lofts.back()->AddWire(nearWires[i]);
                lofts.back()->AddWire(farMatched[i]);
                lofts.back()->CheckCompatibility(false);
                lofts.back()->Build();
                if (!lofts.back()->IsDone()) {
                    kernelFailed("the sweep did not work");
                }
            }
            // A loft from a hole's wire can come out inside out (the wire runs the other way):
            // turned the right way round, it has a positive volume.
            const auto outward = [](TopoDS_Shape solid) {
                GProp_GProps props;
                BRepGProp::VolumeProperties(solid, props);
                if (props.Mass() < 0.0) {
                    solid.Reverse();
                }
                return solid;
            };
            result = outward(lofts.front()->Shape());
            std::vector<std::unique_ptr<BRepAlgoAPI_Cut>> cuts;
            for (std::size_t i = 1; i < lofts.size(); ++i) {
                auto cut = std::make_unique<BRepAlgoAPI_Cut>(result, outward(lofts[i]->Shape()));
                cut->Build();
                if (!cut->IsDone() || cut->HasErrors()) {
                    kernelFailed("the sweep did not work");
                }
                result = cut->Shape();
                cuts.push_back(std::move(cut));
            }
            for (const auto& [edge, indices] : region->edgeCurves) {
                const TopoDS_Shape placedEdge = moved.ModifiedShape(edge);
                for (const auto& loft : lofts) {
                    for (const TopoDS_Shape& side : loft->Generated(placedEdge)) {
                        // The cuts may have rebuilt the face; map to the face in the result.
                        TopoDS_Shape face = side;
                        for (const auto& cut : cuts) {
                            const NCollection_List<TopoDS_Shape>& modified = cut->Modified(face);
                            if (!modified.IsEmpty()) {
                                face = modified.First();
                            }
                        }
                        roles.push_back({face, sideRole(indices, seen)});
                    }
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
                // The caps of an outward taper have no shape to name: a planar face whose
                // normal is parallel to the pull and that lies in one of the two pull planes
                // is a cap.
                const BRepAdaptor_Surface surface(face);
                if (surface.GetType() == GeomAbs_Plane) {
                    const gp_Dir normal = surface.Plane().Axis().Direction();
                    const double parallel = normal.X() * n.x + normal.Y() * n.y + normal.Z() * n.z;
                    if (std::abs(parallel) > 0.999) {
                        for (TopExp_Explorer v(face, TopAbs_VERTEX); v.More(); v.Next()) {
                            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(v.Current()));
                            const double dist = (p.X() - frame.origin.x) * n.x +
                                                (p.Y() - frame.origin.y) * n.y +
                                                (p.Z() - frame.origin.z) * n.z;
                            if (std::abs(dist) < 1e-6) {
                                return "capStart";
                            }
                            if (std::abs(dist - pull) < 1e-6) {
                                return "capEnd";
                            }
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
