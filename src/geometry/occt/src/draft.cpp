// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Draft (AP-2.6): faces are tilted a little about a neutral plane so that a mould lets go of the
// body. The faces keep their names; nothing is made new.

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pln.hxx>

namespace fission::geo::occt {

namespace {

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "draft: a body is empty or was not created by this kernel");
    }
    return data->shape();
}

} // namespace

NamedSolid draftFacesNamed(const NamedSolid& solid, const std::vector<std::uint32_t>& faceIds,
                           Point3d neutralPoint, Point3d pull, double degrees) {
    const double length = std::hypot(pull.x, pull.y, pull.z);
    if (!std::isfinite(degrees) || degrees == 0.0 || std::abs(degrees) >= 45.0) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "draft: needs an angle that is not zero and below 45 degrees", "angle");
    }
    if (faceIds.empty() || !isFinite(neutralPoint) || !isFinite(pull) || !(length > 1e-9)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "draft: needs faces, a neutral plane and a direction");
    }
    const TopoDS_Shape& shape = shapeOf(solid.solid);
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    std::vector<std::string> roles(static_cast<std::size_t>(faces.Extent()));
    for (const NamedFace& face : solid.faces) {
        if (face.faceId >= 1 && face.faceId <= roles.size()) {
            roles[face.faceId - 1] = face.role;
        }
    }
    if (std::any_of(roles.begin(), roles.end(), [](const std::string& r) { return r.empty(); })) {
        throw GeometryError(GeometryErrorCode::invalidInput, "draft: an input face has no role");
    }
    const gp_Dir direction(pull.x / length, pull.y / length, pull.z / length);
    const gp_Pln neutral(gp_Pnt(neutralPoint.x, neutralPoint.y, neutralPoint.z), direction);
    try {
        BRepOffsetAPI_DraftAngle maker(shape);
        for (const std::uint32_t id : faceIds) {
            if (id < 1 || static_cast<int>(id) > faces.Extent()) {
                throw GeometryError(GeometryErrorCode::invalidInput, "draft: a face is not there",
                                    "face");
            }
            maker.Add(TopoDS::Face(faces(static_cast<int>(id))), direction, degrees * M_PI / 180.0,
                      neutral);
            if (!maker.AddDone()) {
                // The face stands square to the pull or lies in the neutral plane.
                throw GeometryError(GeometryErrorCode::invalidInput,
                                    "draft: the face cannot be tilted", "face");
            }
        }
        maker.Build();
        if (!maker.IsDone() || maker.Shape().IsNull()) {
            throw GeometryError(GeometryErrorCode::kernelFailure,
                                "draft: the kernel did not give a result");
        }
        const TopoDS_Shape result = maker.Shape();
        NamedSolid made;
        made.solid = validated(result, "draft");

        ShapeMap finalFaces;
        TopExp::MapShapes(result, TopAbs_FACE, finalFaces);
        std::vector<std::string> finalRoles(static_cast<std::size_t>(finalFaces.Extent()));
        const auto claim = [&](const TopoDS_Shape& piece, const std::string& role) {
            const int index = finalFaces.FindIndex(piece);
            if (index > 0 && finalRoles[static_cast<std::size_t>(index - 1)].empty()) {
                finalRoles[static_cast<std::size_t>(index - 1)] = role;
            }
        };
        for (int i = 1; i <= faces.Extent(); ++i) {
            const TopoDS_Shape& face = faces(i);
            const std::string& role = roles[static_cast<std::size_t>(i - 1)];
            if (maker.IsDeleted(face)) {
                made.history.deleted.push_back(role);
                continue;
            }
            const auto& changed = maker.Modified(face);
            if (changed.IsEmpty()) {
                claim(face, role);
            } else {
                for (const TopoDS_Shape& piece : changed) {
                    claim(piece, role);
                }
            }
        }
        // The kernel hands back faces it did not change as new objects: they are matched to the
        // input faces by kind of surface and place (the nearest face that has no name yet).
        {
            std::set<std::string> claimed(finalRoles.begin(), finalRoles.end());
            claimed.erase("");
            const auto centre = [](const TopoDS_Shape& face) {
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(face, properties);
                return properties.CentreOfMass();
            };
            for (int i = 1; i <= finalFaces.Extent(); ++i) {
                std::string& role = finalRoles[static_cast<std::size_t>(i - 1)];
                if (!role.empty()) {
                    continue;
                }
                const TopoDS_Face now = TopoDS::Face(finalFaces(i));
                const GeomAbs_SurfaceType kind = BRepAdaptor_Surface(now).GetType();
                const gp_Pnt here = centre(now);
                double best = 1e300;
                for (int j = 1; j <= faces.Extent(); ++j) {
                    const std::string& candidate = roles[static_cast<std::size_t>(j - 1)];
                    if (claimed.contains(candidate) ||
                        BRepAdaptor_Surface(TopoDS::Face(faces(j))).GetType() != kind) {
                        continue;
                    }
                    const double distance = centre(faces(j)).Distance(here);
                    if (distance < best) {
                        best = distance;
                        role = candidate;
                    }
                }
                if (!role.empty()) {
                    claimed.insert(role);
                }
            }
        }
        std::set<std::string> seen;
        for (std::string& role : finalRoles) {
            if (role.empty()) {
                throw GeometryError(GeometryErrorCode::kernelFailure,
                                    "draft: a face could not be named");
            }
            std::string candidate = role;
            for (int n = 2; !seen.insert(candidate).second; ++n) {
                candidate = role + "#" + std::to_string(n);
            }
            role = candidate;
        }
        for (std::size_t i = 0; i < finalRoles.size(); ++i) {
            made.faces.push_back(
                {faceNumber(result, finalFaces(static_cast<int>(i) + 1)), finalRoles[i]});
        }
        return made;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "draft: the kernel failed");
    }
}

} // namespace fission::geo::occt
