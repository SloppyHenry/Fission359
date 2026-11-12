// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Drilling a hole (AP-2.7b): one flat face of the one input body, the place of the hole on it
// (u, v in the face's own sketch plane) and the sizes. The kernel builds the cutting tool and
// subtracts it; the result keeps the names of the body.

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A body and its single "face" reference.
const FeatureOutput& bodyOf(const ExecuteInput& input, const char* type) {
    if (input.inputs.size() != 1) {
        throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                 std::string(type) + ": needs exactly one body");
    }
    return input.inputs.front();
}

std::vector<std::uint32_t> facesOf(const ExecuteInput& input, const char* slot, const char* type,
                                   std::size_t most) {
    const auto picked = input.refs.find(slot);
    if (picked == input.refs.end() || picked->second.empty() || picked->second.size() > most) {
        throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                 std::string(type) + ": needs the reference '" + slot + "'");
    }
    std::vector<std::uint32_t> ids;
    for (const ResolvedRef& ref : picked->second) {
        ids.push_back(ref.id);
    }
    return ids;
}

FeatureResult resultOf(const ExecuteInput& input, geo::NamedSolid made) {
    FeatureResult result;
    result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                              std::move(made.faces), std::move(made.history)});
    return result;
}

std::optional<FeatureIssue> outOfRange(const char* name) {
    FeatureIssue issue;
    issue.code = FeatureIssueCode::parameterOutOfRange;
    issue.subject = name;
    return issue;
}

std::optional<geo::HoleKind> kindOf(const std::string& word) {
    if (word == "plain") {
        return geo::HoleKind::plain;
    }
    if (word == "counterbore") {
        return geo::HoleKind::counterbore;
    }
    if (word == "countersink") {
        return geo::HoleKind::countersink;
    }
    if (word == "nutTrap") {
        return geo::HoleKind::nutTrap;
    }
    return std::nullopt;
}

class HoleType final : public FeatureType {
public:
    std::string_view id() const override { return "hole"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        // `u` and `v` place the hole on the face, in the plane of its sketch (the face's centre
        // is (0, 0)); the sizes are the hole's and its head's.
        return {{"u", QuantityKind::length, "0"},
                {"v", QuantityKind::length, "0"},
                {"diameter", QuantityKind::length, "3.4"},
                {"depth", QuantityKind::length, "10"},
                {"headDiameter", QuantityKind::length, "0"},
                {"headDepth", QuantityKind::length, "0"},
                {"nutWidth", QuantityKind::length, "0"},
                {"nutDepth", QuantityKind::length, "0"}};
    }

    std::vector<TextFieldSpec> texts() const override { return {{"kind", "plain"}}; }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        if (auto issue = requirePositive(params, {"diameter", "depth"})) {
            return issue;
        }
        // The head and nut measures count only when they are set (the word `kind` says which);
        // the kernel refuses the combinations that do not fit (requireFitting).
        const double diameter = valueOf(params, "diameter");
        if (const double headDiameter = valueOf(params, "headDiameter");
            headDiameter > 0.0 && !(headDiameter > diameter)) {
            return outOfRange("headDiameter");
        }
        if (const double nutWidth = valueOf(params, "nutWidth");
            nutWidth > 0.0 && !(valueOf(params, "nutDepth") > 0.0)) {
            return outOfRange("nutDepth");
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const FeatureOutput& body = bodyOf(input, "hole");
        const std::uint32_t faceId = facesOf(input, "face", "hole", 1).front();
        const std::string kindWord = textOf(input.feature, "kind", "plain");
        const std::optional<geo::HoleKind> kind = kindOf(kindWord);
        if (!kind) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "hole: unknown kind '" + kindWord + "'");
        }
        // The plane of the face, worked out from the face itself, so the hole follows the body.
        const geo::FlatFace face = context.kernel.flatFace(body.solid, faceId);
        const SketchPlane plane = planeOnFace(face.centre, face.normal, face.longestEdge);
        geo::HoleToolSpec spec;
        spec.kind = *kind;
        spec.diameter = valueOf(input.params, "diameter");
        spec.depth = valueOf(input.params, "depth");
        spec.headDiameter = valueOf(input.params, "headDiameter");
        spec.headDepth = valueOf(input.params, "headDepth");
        spec.nutWidth = valueOf(input.params, "nutWidth");
        spec.nutDepth = valueOf(input.params, "nutDepth");
        spec.at = plane.toWorld({valueOf(input.params, "u"), valueOf(input.params, "v")});
        spec.up = face.normal;
        const geo::NamedSolid tool = context.kernel.makeHoleTool(spec);
        const geo::NamedSolid whole{body.solid, body.faces};
        return resultOf(input,
                        context.kernel.boolean(geo::BooleanOp::subtract, whole, {tool}, 0.0));
    }

    Description describe(const ResolvedParams& params) const override {
        return {"hole", {params.at("diameter"), params.at("depth")}};
    }

    bool passesOnNames() const override { return true; }
    // The place and the sizes are typed in the panel, there is no single place for a grip.
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
    // `u` and `v` place the hole, they are no sizes of it.
    std::vector<std::string> sizeParameters() const override {
        return {"diameter", "depth", "headDiameter", "headDepth", "nutWidth", "nutDepth"};
    }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeHoleType() {
    return std::make_unique<HoleType>();
}
} // namespace detail

} // namespace fission::features
