// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Rounding and bevelling edges (AP-2.5) with the naming contract: faces keep their roles, the
// faces made along an edge are named after that edge.

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Edge.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kMaxSize = 1.0e6;

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "blend: a body is empty or was not created by this kernel");
    }
    return data->shape();
}

bool usable(double size) {
    return std::isfinite(size) && size > 0.0 && size < kMaxSize;
}

TopoDS_Edge edgeOf(const ShapeMap& edges, std::uint32_t id) {
    if (id < 1 || static_cast<int>(id) > edges.Extent()) {
        throw GeometryError(GeometryErrorCode::invalidInput, "blend: an edge is not in the body");
    }
    return TopoDS::Edge(edges(static_cast<int>(id)));
}

} // namespace

NamedSolid blendEdgesNamed(const NamedSolid& solid, const std::vector<BlendEdge>& blendEdges,
                           const BlendSpec& spec, const std::string& rolePrefix) {
    if (!usable(spec.size) || !(spec.size2 >= 0.0) || !std::isfinite(spec.size2) ||
        blendEdges.empty()) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "blend: needs a positive size and at least one edge");
    }
    const TopoDS_Shape& shape = shapeOf(solid.solid);
    ShapeMap edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    std::vector<std::string> roles(static_cast<std::size_t>(faces.Extent()));
    for (const NamedFace& face : solid.faces) {
        if (face.faceId >= 1 && face.faceId <= roles.size()) {
            roles[face.faceId - 1] = face.role;
        }
    }
    if (std::any_of(roles.begin(), roles.end(),
                    [](const std::string& role) { return role.empty(); })) {
        throw GeometryError(GeometryErrorCode::invalidInput, "blend: an input face has no role");
    }

    try {
        std::unique_ptr<BRepBuilderAPI_MakeShape> maker;
        if (spec.kind == BlendKind::round) {
            auto fillet = std::make_unique<BRepFilletAPI_MakeFillet>(shape);
            for (const BlendEdge& edge : blendEdges) {
                fillet->Add(spec.size, edgeOf(edges, edge.id));
            }
            maker = std::move(fillet);
        } else {
            auto chamfer = std::make_unique<BRepFilletAPI_MakeChamfer>(shape);
            for (const BlendEdge& edge : blendEdges) {
                const TopoDS_Edge picked = edgeOf(edges, edge.id);
                if (spec.size2 > 0.0) {
                    // The distance `size` is measured on a face next to the edge.
                    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>
                        ancestors;
                    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, ancestors);
                    const auto& next = ancestors.FindFromKey(picked);
                    chamfer->Add(spec.size, spec.size2, picked, TopoDS::Face(next.First()));
                } else {
                    chamfer->Add(spec.size, picked);
                }
            }
            maker = std::move(chamfer);
        }
        maker->Build();
        if (!maker->IsDone() || maker->Shape().IsNull()) {
            throw GeometryError(GeometryErrorCode::blendTooLarge,
                                "blend: the kernel could not round these edges", "size");
        }
        const TopoDS_Shape result = maker->Shape();

        NamedSolid named;
        try {
            named.solid = validated(result, "blend");
        } catch (const GeometryError& error) {
            if (error.code() != GeometryErrorCode::invalidResult) {
                throw;
            }
            GeometryError failed(GeometryErrorCode::blendTooLarge, error.what(), "size");
            failed.withPreview(error.preview());
            throw failed;
        }

        // Names: old faces keep theirs; a face made along an edge is named after the edge.
        ShapeMap finalFaces;
        TopExp::MapShapes(result, TopAbs_FACE, finalFaces);
        std::vector<std::string> finalRoles(static_cast<std::size_t>(finalFaces.Extent()));
        const auto claim = [&](const TopoDS_Shape& piece, const std::string& role) {
            const int index = finalFaces.FindIndex(piece);
            if (index > 0 && finalRoles[static_cast<std::size_t>(index - 1)].empty()) {
                finalRoles[static_cast<std::size_t>(index - 1)] = role;
            }
        };
        ElementHistory history;
        for (int i = 1; i <= faces.Extent(); ++i) {
            const TopoDS_Shape& face = faces(i);
            const std::string& role = roles[static_cast<std::size_t>(i - 1)];
            if (maker->IsDeleted(face)) {
                history.deleted.push_back(role);
                continue;
            }
            const auto& changed = maker->Modified(face);
            if (changed.IsEmpty()) {
                claim(face, role);
            } else {
                for (const TopoDS_Shape& piece : changed) {
                    claim(piece, role);
                }
            }
        }
        std::set<std::string> taken(finalRoles.begin(), finalRoles.end());
        const auto unique = [&](std::string role) {
            std::string candidate = role;
            for (int n = 2; taken.contains(candidate); ++n) {
                candidate = role + "#" + std::to_string(n);
            }
            taken.insert(candidate);
            return candidate;
        };
        for (const BlendEdge& edge : blendEdges) {
            const std::string label = edge.name.empty() ? "e" + std::to_string(edge.id) : edge.name;
            for (const TopoDS_Shape& piece : maker->Generated(edges(static_cast<int>(edge.id)))) {
                const int index = finalFaces.FindIndex(piece);
                if (index > 0 && finalRoles[static_cast<std::size_t>(index - 1)].empty()) {
                    finalRoles[static_cast<std::size_t>(index - 1)] =
                        unique(rolePrefix + "[" + label + "]");
                }
            }
        }
        int corner = 0;
        for (std::string& role : finalRoles) {
            if (role.empty()) { // a patch where rounded edges meet
                role = unique(rolePrefix + "#corner" + std::to_string(++corner));
            }
        }
        for (std::size_t i = 0; i < finalRoles.size(); ++i) {
            named.faces.push_back(
                {faceNumber(result, finalFaces(static_cast<int>(i) + 1)), finalRoles[i]});
        }
        named.history = std::move(history);
        return named;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::blendTooLarge,
                            "blend: the kernel failed on these edges", "size");
    }
}

std::vector<std::uint32_t> tangentChainOf(const Solid& solid, std::uint32_t edge) {
    const TopoDS_Shape& shape = shapeOf(solid);
    ShapeMap edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    try {
        BRepFilletAPI_MakeFillet probe(shape);
        probe.Add(0.001, edgeOf(edges, edge));
        std::vector<std::uint32_t> chain;
        for (int contour = 1; contour <= probe.NbContours(); ++contour) {
            for (int at = 1; at <= probe.NbEdges(contour); ++at) {
                const int index = edges.FindIndex(probe.Edge(contour, at));
                if (index > 0) {
                    chain.push_back(static_cast<std::uint32_t>(index));
                }
            }
        }
        if (std::find(chain.begin(), chain.end(), edge) == chain.end()) {
            chain.push_back(edge);
        }
        std::sort(chain.begin(), chain.end());
        chain.erase(std::unique(chain.begin(), chain.end()), chain.end());
        return chain;
    } catch (const Standard_Failure&) {
        return {edge};
    }
}

} // namespace fission::geo::occt
