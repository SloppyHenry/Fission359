// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Private to src/geometry/occt: what the body builders (solids.cpp, primitives.cpp) share.

#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/kernel.h"
#include "fission/geo/types.h"

#include "occt_solid.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace fission::geo::occt {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

inline bool isPositiveFinite(double value) {
    return std::isfinite(value) && value > 0.0;
}

inline bool isFinite(const Point3d& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

// Results are validated before anybody sees them: an invalid body is an error, not a warning
// (master prompt, rule 8).
inline Solid validated(const TopoDS_Shape& shape, const char* what) {
    if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid()) {
        throw GeometryError(GeometryErrorCode::invalidResult,
                            std::string(what) + ": the kernel returned an invalid body");
    }
    return wrap(shape);
}

// 1-based number of `face` among the faces of `shape`: the same numbering tessellate() uses.
inline std::uint32_t faceNumber(const TopoDS_Shape& shape, const TopoDS_Shape& face) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    const int index = faces.FindIndex(face);
    if (index <= 0) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "a primitive face could not be found in its body");
    }
    return static_cast<std::uint32_t>(index);
}

// The roles must cover every face exactly once (naming contract, ADR-0003).
inline NamedSolid withRoles(const TopoDS_Shape& shape, const char* what,
                            const std::vector<std::pair<std::string, TopoDS_Shape>>& roles) {
    NamedSolid result;
    result.solid = validated(shape, what);
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (static_cast<std::size_t>(faces.Extent()) != roles.size()) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            std::string(what) + ": not every face got a role");
    }
    for (const auto& [role, face] : roles) {
        result.faces.push_back({faceNumber(shape, face), role});
    }
    return result;
}

// Names every face of `shape` with `roleOf(face)`. An empty role is a bug of the builder and
// ends in an error (the contract is never silently broken).
inline NamedSolid nameFaces(const TopoDS_Shape& shape, const char* what,
                            const std::function<std::string(const TopoDS_Face&)>& roleOf) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    std::vector<std::pair<std::string, TopoDS_Shape>> roles;
    for (int i = 1; i <= faces.Extent(); ++i) {
        std::string role = roleOf(TopoDS::Face(faces(i)));
        if (role.empty()) {
            throw GeometryError(GeometryErrorCode::kernelFailure,
                                std::string(what) + ": a face could not be named");
        }
        roles.emplace_back(std::move(role), faces(i));
    }
    return withRoles(shape, what, roles);
}

// The direction a flat face looks to (out of the body). Meaningless for curved faces.
inline gp_Dir outwardNormal(const TopoDS_Face& face) {
    const BRepAdaptor_Surface surface(face);
    gp_Dir normal = surface.Plane().Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }
    return normal;
}

inline bool isFlat(const TopoDS_Face& face) {
    return BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane;
}

// Centre of mass of the face's surface.
inline gp_Pnt faceCentre(const TopoDS_Face& face) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return properties.CentreOfMass();
}

} // namespace fission::geo::occt
