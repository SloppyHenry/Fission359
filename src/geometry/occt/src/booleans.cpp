// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Merging, subtracting and intersecting bodies (AP-1.7, ADR-0011), with the naming contract:
// every face of the result keeps the role of the input face it comes from.

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepTools_History.hxx>
#include <NCollection_List.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Builder.hxx>
#include <TopoDS_Compound.hxx>

namespace fission::geo::occt {

namespace {

// What happened to a face on the way from the inputs to the result.
struct Lineage {
    // For each face of the shape, the roles it received and the rank of the input it came from
    // (0 = target, 1.. = tools): the lowest rank names the face.
    struct Claim {
        int rank = 0;
        std::string role;
    };
    std::vector<std::vector<Claim>> byFace; // indexed by (face number - 1)
};

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "boolean: a body is empty or was not created by this kernel");
    }
    return data->shape();
}

int solidCount(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
        ++count;
    }
    return count;
}

// The solids of the result as one body: the solid itself, or a compound of them.
TopoDS_Shape onlySolids(const TopoDS_Shape& shape) {
    if (solidCount(shape) == 1) {
        TopExp_Explorer it(shape, TopAbs_SOLID);
        return it.Current();
    }
    TopoDS_Compound compound;
    TopoDS_Builder builder;
    builder.MakeCompound(compound);
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
        builder.Add(compound, it.Current());
    }
    return compound;
}

// Looks every face of `inputs` up in the result of `algo`; a face that was changed leads to
// the faces it became, one that was not changed to itself.
template <typename History>
void trace(History& history, const ShapeMap& inputFaces, const std::vector<std::string>& roles,
           int rank, const ShapeMap& resultFaces, Lineage& lineage) {
    for (int i = 1; i <= inputFaces.Extent(); ++i) {
        const TopoDS_Shape& face = inputFaces(i);
        if (history.deleted(face)) {
            continue;
        }
        const NCollection_List<TopoDS_Shape>& changed = history.modified(face);
        const auto claim = [&](const TopoDS_Shape& result) {
            const int index = resultFaces.FindIndex(result);
            if (index > 0) {
                lineage.byFace[static_cast<std::size_t>(index - 1)].push_back(
                    {rank, roles[static_cast<std::size_t>(i - 1)]});
            }
        };
        if (changed.IsEmpty()) {
            claim(face);
        } else {
            for (const TopoDS_Shape& piece : changed) {
                claim(piece);
            }
        }
    }
}

struct AlgoHistory {
    BRepAlgoAPI_BuilderAlgo& algo;
    bool deleted(const TopoDS_Shape& s) { return algo.IsDeleted(s); }
    const NCollection_List<TopoDS_Shape>& modified(const TopoDS_Shape& s) {
        return algo.Modified(s);
    }
};

struct UnifyHistory {
    const occ::handle<BRepTools_History>& history;
    bool deleted(const TopoDS_Shape& s) { return history->IsRemoved(s); }
    const NCollection_List<TopoDS_Shape>& modified(const TopoDS_Shape& s) {
        return history->Modified(s);
    }
};

// The role of a face is the one the lowest-ranked input gave it; ties go to the first claim.
std::vector<std::pair<int, std::string>> decide(const Lineage& lineage, const char* what) {
    std::vector<std::pair<int, std::string>> decided;
    for (const auto& claims : lineage.byFace) {
        if (claims.empty()) {
            throw GeometryError(GeometryErrorCode::kernelFailure,
                                std::string(what) + ": a face could not be traced to its source");
        }
        const auto best = std::min_element(
            claims.begin(), claims.end(),
            [](const Lineage::Claim& a, const Lineage::Claim& b) { return a.rank < b.rank; });
        decided.emplace_back(best->rank, best->role);
    }
    return decided;
}

} // namespace

NamedSolid booleanNamed(BooleanOp op, const NamedSolid& target,
                        const std::vector<NamedSolid>& tools, double fuzzy) {
    if (tools.empty() || !std::isfinite(fuzzy) || fuzzy < 0.0) {
        throw GeometryError(GeometryErrorCode::invalidInput, "boolean: no tool given");
    }
    try {
        std::vector<const NamedSolid*> inputs{&target};
        for (const NamedSolid& tool : tools) {
            inputs.push_back(&tool);
        }

        NCollection_List<TopoDS_Shape> argumentList;
        NCollection_List<TopoDS_Shape> toolList;
        argumentList.Append(shapeOf(target.solid));
        for (const NamedSolid& tool : tools) {
            toolList.Append(shapeOf(tool.solid));
        }

        // Exact (no fuzzy value), on one thread, inputs left untouched: they are shared.
        std::unique_ptr<BRepAlgoAPI_BooleanOperation> algo;
        switch (op) {
        case BooleanOp::merge:
            algo = std::make_unique<BRepAlgoAPI_Fuse>();
            break;
        case BooleanOp::subtract:
            algo = std::make_unique<BRepAlgoAPI_Cut>();
            break;
        case BooleanOp::intersect:
            algo = std::make_unique<BRepAlgoAPI_Common>();
            break;
        }
        algo->SetArguments(argumentList);
        algo->SetTools(toolList);
        algo->SetRunParallel(false);
        if (fuzzy > 0.0) {
            algo->SetFuzzyValue(fuzzy);
        }
        algo->SetNonDestructive(true);
        algo->Build();
        if (!algo->IsDone() || algo->HasErrors() || algo->Shape().IsNull()) {
            throw GeometryError(GeometryErrorCode::kernelFailure, "boolean: the kernel failed");
        }
        if (solidCount(algo->Shape()) == 0) {
            throw GeometryError(GeometryErrorCode::emptyResult, "boolean: nothing is left");
        }

        // Join faces that lie in one surface (a hole's rim next to a flat top stays two).
        ShapeUpgrade_UnifySameDomain unify(algo->Shape(), /*UnifyEdges=*/true, /*UnifyFaces=*/true,
                                           /*ConcatBSplines=*/false);
        unify.Build();
        const TopoDS_Shape result = onlySolids(unify.Shape());

        // Names: input face -> face after the operation -> face after joining.
        ShapeMap beforeUnify;
        TopExp::MapShapes(algo->Shape(), TopAbs_FACE, beforeUnify);
        Lineage afterOperation;
        afterOperation.byFace.resize(static_cast<std::size_t>(beforeUnify.Extent()));
        AlgoHistory history{*algo};
        for (std::size_t rank = 0; rank < inputs.size(); ++rank) {
            const NamedSolid& input = *inputs[rank];
            const TopoDS_Shape& shape = shapeOf(input.solid);
            ShapeMap faces;
            TopExp::MapShapes(shape, TopAbs_FACE, faces);
            std::vector<std::string> roles(static_cast<std::size_t>(faces.Extent()));
            for (const NamedFace& face : input.faces) {
                if (face.faceId >= 1 && face.faceId <= roles.size()) {
                    roles[face.faceId - 1] = face.role;
                }
            }
            if (std::any_of(roles.begin(), roles.end(),
                            [](const std::string& role) { return role.empty(); })) {
                throw GeometryError(GeometryErrorCode::invalidInput,
                                    "boolean: an input face has no role");
            }
            trace(history, faces, roles, static_cast<int>(rank), beforeUnify, afterOperation);
        }
        const auto operationRoles = decide(afterOperation, "boolean");

        ShapeMap finalFaces;
        TopExp::MapShapes(result, TopAbs_FACE, finalFaces);
        Lineage afterJoin;
        afterJoin.byFace.resize(static_cast<std::size_t>(finalFaces.Extent()));
        {
            UnifyHistory joined{unify.History()};
            // The rank of a joined face is the rank of the face that names it, so the target
            // keeps its names when it is merged with a tool.
            for (int i = 1; i <= beforeUnify.Extent(); ++i) {
                const TopoDS_Shape& face = beforeUnify(i);
                const auto& [rank, role] = operationRoles[static_cast<std::size_t>(i - 1)];
                if (joined.deleted(face)) {
                    continue;
                }
                const NCollection_List<TopoDS_Shape>& changed = joined.modified(face);
                const auto claim = [&](const TopoDS_Shape& piece) {
                    const int index = finalFaces.FindIndex(piece);
                    if (index > 0) {
                        afterJoin.byFace[static_cast<std::size_t>(index - 1)].push_back(
                            {rank, role});
                    }
                };
                if (changed.IsEmpty()) {
                    claim(face);
                } else {
                    for (const TopoDS_Shape& piece : changed) {
                        claim(piece);
                    }
                }
            }
        }
        const auto finalRoles = decide(afterJoin, "boolean");

        // One input face cut into pieces: the pieces share its role, so they are numbered.
        std::map<std::string, int> uses;
        for (const auto& [rank, role] : finalRoles) {
            ++uses[role];
        }
        std::map<std::string, int> counter;
        NamedSolid named;
        named.solid = validated(result, "boolean");
        for (std::size_t i = 0; i < finalRoles.size(); ++i) {
            const std::string& role = finalRoles[i].second;
            std::string name = role;
            if (uses[role] > 1) {
                name += "#" + std::to_string(++counter[role]);
            }
            named.faces.push_back({faceNumber(result, finalFaces(static_cast<int>(i) + 1)), name});
        }
        return named;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "boolean: the kernel failed");
    }
}

} // namespace fission::geo::occt
