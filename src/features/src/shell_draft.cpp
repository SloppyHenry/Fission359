// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Hollowing and draft (AP-2.6). Both take one body and faces as persistent references (ADR-0003)
// and give back one body that carries the names of its input.

#include <cmath>
#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

std::vector<std::uint32_t> idsOf(const ExecuteInput& input, const char* slot) {
    std::vector<std::uint32_t> ids;
    const auto picked = input.refs.find(slot);
    if (picked != input.refs.end()) {
        for (const ResolvedRef& ref : picked->second) {
            ids.push_back(ref.id);
        }
    }
    return ids;
}

std::size_t countOf(const Feature& feature, const char* slot) {
    const auto found = feature.refs.find(slot);
    return found == feature.refs.end() ? 0 : found->second.size();
}

// --- hollowing ----------------------------------------------------------------------------

class ShellType final : public FeatureType {
public:
    std::string_view id() const override { return "shell"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        // Where the grip stands: a point on the first open face and the way into the body (all 0 =
        // no grip). Set when the step is made; the person does not edit them (they are not shown).
        std::vector<ParamSpec> own{{"thickness", QuantityKind::length, "2"}};
        for (const char* name : {"anchorX", "anchorY", "anchorZ"}) {
            own.push_back({name, QuantityKind::length, "0"});
        }
        for (const char* name : {"anchorDx", "anchorDy", "anchorDz"}) {
            own.push_back({name, QuantityKind::plain, "0"});
        }
        return own;
    }

    // "side": "inside" makes the wall inside the body, "outside" around it.
    std::vector<TextFieldSpec> texts() const override { return {{"side", "inside"}}; }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return requirePositive(params, {"thickness"});
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        if (input.inputs.size() != 1) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "shell: needs exactly one body");
        }
        const FeatureOutput& body = input.inputs.front();
        const geo::NamedSolid whole{body.solid, body.faces};
        const bool outside = textOf(input.feature, "side", "inside") == "outside";
        geo::NamedSolid made = context.kernel.hollow(whole, idsOf(input, "openFaces"),
                                                     valueOf(input.params, "thickness"), outside,
                                                     elementName(input.feature, "inner"));
        FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                                  std::move(made.faces), std::move(made.history)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"shell", {Quantity{0.0, {}}, params.at("thickness")}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        return {"shell",
                {Quantity{double(countOf(feature, "openFaces")), {}}, params.at("thickness")}};
    }

    bool passesOnNames() const override { return true; }

    // The grip sits as deep inside as the wall is thick; dragging it makes the wall thicker.
    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const geo::Point3d way{valueOf(params, "anchorDx"), valueOf(params, "anchorDy"),
                               valueOf(params, "anchorDz")};
        const double length = std::hypot(way.x, way.y, way.z);
        if (!(length > 0.5)) {
            return {};
        }
        const double thickness = valueOf(params, "thickness");
        const geo::Point3d unit{way.x / length, way.y / length, way.z / length};
        const geo::Point3d spot{valueOf(params, "anchorX"), valueOf(params, "anchorY"),
                                valueOf(params, "anchorZ")};
        return {makeHandle(
            "thickness", HandleKind::distance,
            {spot.x + unit.x * thickness, spot.y + unit.y * thickness, spot.z + unit.z * thickness},
            unit, thickness)};
    }

    std::vector<std::string> sizeParameters() const override { return {"thickness"}; }
};

// --- draft --------------------------------------------------------------------------------

class DraftType final : public FeatureType {
public:
    std::string_view id() const override { return "draft"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        // The neutral plane: a point on it and the way the mould is pulled (set when the step is
        // made). The angle is typed.
        std::vector<ParamSpec> own{{"angle", QuantityKind::angle, "3"}};
        for (const char* name : {"neutralX", "neutralY", "neutralZ"}) {
            own.push_back({name, QuantityKind::length, "0"});
        }
        own.push_back({"pullX", QuantityKind::plain, "0"});
        own.push_back({"pullY", QuantityKind::plain, "0"});
        own.push_back({"pullZ", QuantityKind::plain, "1"});
        return own;
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const double angle = valueOf(params, "angle");
        if (!std::isfinite(angle) || angle == 0.0) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "angle";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const std::vector<std::uint32_t> faces = idsOf(input, "faces");
        if (input.inputs.size() != 1 || faces.empty()) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "draft: needs one body and the reference 'faces'");
        }
        const FeatureOutput& body = input.inputs.front();
        const geo::NamedSolid whole{body.solid, body.faces};
        const auto at = [&](const char* x, const char* y, const char* z) {
            return geo::Point3d{valueOf(input.params, x), valueOf(input.params, y),
                                valueOf(input.params, z)};
        };
        geo::NamedSolid made = context.kernel.draftFaces(
            whole, faces, at("neutralX", "neutralY", "neutralZ"), at("pullX", "pullY", "pullZ"),
            valueOf(input.params, "angle"));
        FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                                  std::move(made.faces), std::move(made.history)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"draft", {Quantity{0.0, {}}, params.at("angle")}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        return {"draft", {Quantity{double(countOf(feature, "faces")), {}}, params.at("angle")}};
    }

    bool passesOnNames() const override { return true; }
    // An arc grip for the angle does not exist yet: it is typed in the panel.
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeShellType() {
    return std::make_unique<ShellType>();
}
std::unique_ptr<const core::FeatureType> makeDraftType() {
    return std::make_unique<DraftType>();
}
} // namespace detail

} // namespace fission::features
