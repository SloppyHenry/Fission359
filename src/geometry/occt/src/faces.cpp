// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pushing and pulling a face (AP-2.4a): a flat face moves along its normal, the wall of a round
// hole or pin changes its radius. Both are made exactly with a prism or a cylinder that is
// added to the body or cut from it; the face keeps its name.

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Ax2.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kParallel = 1e-6; // sine of the angle that still counts as square

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "offsetFace: a body is empty or was not created by this kernel");
    }
    return data->shape();
}

// The faces beside `face` all stand square on it (a flat one at a right angle, a round one around
// the direction of the push). Only then does a prism give the right body; a slanted neighbour
// would have to be stretched, which this does not do.
bool neighboursSquare(const TopoDS_Shape& shape, const TopoDS_Face& face, const gp_Dir& push) {
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                               TopTools_ShapeMapHasher>
        beside;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, beside);
    for (TopExp_Explorer edges(face, TopAbs_EDGE); edges.More(); edges.Next()) {
        for (const TopoDS_Shape& other : beside.FindFromKey(edges.Current())) {
            if (other.IsSame(face)) {
                continue;
            }
            const BRepAdaptor_Surface surface(TopoDS::Face(other));
            if (surface.GetType() == GeomAbs_Plane) {
                if (std::abs(surface.Plane().Axis().Direction().Dot(push)) > kParallel) {
                    return false;
                }
            } else if (surface.GetType() == GeomAbs_Cylinder) {
                if (gp_Vec(surface.Cylinder().Axis().Direction())
                        .Crossed(gp_Vec(push))
                        .Magnitude() > kParallel) {
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    return true;
}

// A tool for combining with the body: the face that becomes the moved wall carries the wall's own
// role (`wall` tells which one it is), every other face the role with "[push]" - those join
// faces of the body or vanish into it.
NamedSolid tool(const TopoDS_Shape& shape, const std::string& role,
                const std::function<bool(const TopoDS_Face&)>& wall) {
    return nameFaces(shape, "offsetFace",
                     [&](const TopoDS_Face& face) { return wall(face) ? role : role + "[push]"; });
}

NamedSolid pushFlat(const NamedSolid& solid, const TopoDS_Shape& shape, const TopoDS_Face& face,
                    const std::string& role, double distance) {
    const gp_Dir normal = outwardNormal(face);
    if (!neighboursSquare(shape, face, normal)) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "offsetFace: a slanted neighbour of the face would have to stretch");
    }
    const gp_Vec along(normal.XYZ() * distance);
    BRepPrimAPI_MakePrism prism(face, along);
    prism.Build();
    if (!prism.IsDone()) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "offsetFace: no prism");
    }
    const BooleanOp op = distance > 0.0 ? BooleanOp::merge : BooleanOp::subtract;
    const gp_Pnt target(faceCentre(face).XYZ() + normal.XYZ() * distance);
    const auto isWall = [&](const TopoDS_Face& candidate) {
        return isFlat(candidate) && faceCentre(candidate).Distance(target) < 1e-6;
    };
    return booleanNamed(op, solid, {tool(prism.Shape(), role, isWall)});
}

// Where a round wall is looked at from and whether it is a pin (looks away from the axis) or a
// hole (looks towards it).
struct RoundWall {
    gp_Cylinder cylinder;
    double u0 = 0.0, u1 = 0.0, v0 = 0.0, v1 = 0.0;
    gp_Pnt point; // on the wall, half way round and along
    gp_Vec radial;
    bool pin = false;
};

RoundWall roundWallOf(const TopoDS_Face& face) {
    RoundWall wall;
    const BRepAdaptor_Surface surface(face);
    wall.cylinder = surface.Cylinder();
    BRepTools::UVBounds(face, wall.u0, wall.u1, wall.v0, wall.v1);
    if (std::abs((wall.u1 - wall.u0) - 2.0 * M_PI) > 1e-6) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "offsetFace: only a whole round wall can change its radius");
    }
    gp_Vec du, dv;
    surface.D1(0.5 * (wall.u0 + wall.u1), 0.5 * (wall.v0 + wall.v1), wall.point, du, dv);
    gp_Vec out = du.Crossed(dv);
    if (face.Orientation() == TopAbs_REVERSED) {
        out.Reverse();
    }
    const gp_Vec axis(wall.cylinder.Axis().Direction());
    const gp_Vec fromBase(wall.cylinder.Location(), wall.point);
    wall.radial = fromBase - axis * fromBase.Dot(axis);
    wall.pin = out.Dot(wall.radial) > 0.0;
    return wall;
}

NamedSolid pushRound(const NamedSolid& solid, const TopoDS_Face& face, const std::string& role,
                     double distance) {
    const RoundWall wall = roundWallOf(face);
    const gp_Cylinder& cylinder = wall.cylinder;
    const double v0 = wall.v0;
    const double v1 = wall.v1;
    const bool pin = wall.pin;
    const double radius = cylinder.Radius();
    const double wanted = pin ? radius + distance : radius - distance;
    if (!(wanted > 1e-6)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "offsetFace: the radius would drop to zero", "distance");
    }
    const gp_Ax2 frame(
        gp_Pnt(cylinder.Position().Location().XYZ() + cylinder.Axis().Direction().XYZ() * v0),
        cylinder.Axis().Direction());
    const double height = v1 - v0;
    const auto makeCylinder = [&](double r) {
        return BRepPrimAPI_MakeCylinder(frame, r, height).Shape();
    };
    const bool growsMaterial = distance > 0.0;
    const double inner = std::min(radius, wanted);
    const double outer = std::max(radius, wanted);
    TopoDS_Shape added;
    BooleanOp op;
    if ((pin && growsMaterial) || (!pin && !growsMaterial)) {
        // The body gets more material (a thicker pin) or loses more (a wider hole): a full
        // cylinder of the new radius does it, the old wall lies inside it.
        added = makeCylinder(wanted);
        op = pin ? BooleanOp::merge : BooleanOp::subtract;
    } else {
        // A thinner pin or a narrower hole: a tube between the two radii is cut off or filled.
        BRepAlgoAPI_Cut tube(makeCylinder(outer), makeCylinder(inner));
        added = tube.Shape();
        op = pin ? BooleanOp::subtract : BooleanOp::merge;
    }
    const auto isWall = [&](const TopoDS_Face& candidate) {
        const BRepAdaptor_Surface looks(candidate);
        return looks.GetType() == GeomAbs_Cylinder &&
               std::abs(looks.Cylinder().Radius() - wanted) < 1e-9;
    };
    return booleanNamed(op, solid, {tool(added, role, isWall)});
}

} // namespace

NamedSolid offsetFaceNamed(const NamedSolid& solid, std::uint32_t faceId, double distance) {
    if (!std::isfinite(distance) || distance == 0.0 || std::abs(distance) > 1.0e6) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "offsetFace: needs a distance that is not zero", "distance");
    }
    const TopoDS_Shape& shape = shapeOf(solid.solid);
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (faceId < 1 || static_cast<int>(faceId) > faces.Extent()) {
        throw GeometryError(GeometryErrorCode::invalidInput, "offsetFace: the face is not there");
    }
    const auto own = std::find_if(solid.faces.begin(), solid.faces.end(),
                                  [&](const NamedFace& f) { return f.faceId == faceId; });
    if (own == solid.faces.end() || own->role.empty()) {
        throw GeometryError(GeometryErrorCode::invalidInput, "offsetFace: the face has no role");
    }
    const TopoDS_Face face = TopoDS::Face(faces(static_cast<int>(faceId)));
    try {
        switch (BRepAdaptor_Surface(face).GetType()) {
        case GeomAbs_Plane:
            return pushFlat(solid, shape, face, own->role, distance);
        case GeomAbs_Cylinder:
            return pushRound(solid, face, own->role, distance);
        default:
            throw GeometryError(GeometryErrorCode::kernelFailure,
                                "offsetFace: only flat and cylindrical faces can be moved");
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "offsetFace: the kernel failed");
    }
}

FaceAnchor faceAnchorOf(const Solid& solid, std::uint32_t faceId) {
    ShapeMap faces;
    TopExp::MapShapes(shapeOf(solid), TopAbs_FACE, faces);
    if (faceId < 1 || static_cast<int>(faceId) > faces.Extent()) {
        throw GeometryError(GeometryErrorCode::invalidInput, "faceAnchor: the face is not there");
    }
    const TopoDS_Face face = TopoDS::Face(faces(static_cast<int>(faceId)));
    try {
        switch (BRepAdaptor_Surface(face).GetType()) {
        case GeomAbs_Plane: {
            const gp_Pnt middle = faceCentre(face);
            const gp_Dir normal = outwardNormal(face);
            return {{middle.X(), middle.Y(), middle.Z()}, {normal.X(), normal.Y(), normal.Z()}};
        }
        case GeomAbs_Cylinder: {
            const RoundWall wall = roundWallOf(face);
            gp_Vec way = wall.radial;
            if (way.Magnitude() < 1e-12) {
                throw GeometryError(GeometryErrorCode::kernelFailure,
                                    "faceAnchor: a round wall without a radius");
            }
            way.Normalize();
            if (!wall.pin) {
                way.Reverse(); // a hole looks towards its axis
            }
            return {{wall.point.X(), wall.point.Y(), wall.point.Z()}, {way.X(), way.Y(), way.Z()}};
        }
        default:
            throw GeometryError(GeometryErrorCode::kernelFailure,
                                "faceAnchor: only flat and cylindrical faces can be moved");
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "faceAnchor: the kernel failed");
    }
}

} // namespace fission::geo::occt
