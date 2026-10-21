// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Combining bodies (AP-1.7, ADR-0011): merge, subtract (also what a hole does) and intersect.
// `inputs[0]` is the target, `inputs[1..]` are the tools; the one output is the result.

#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

std::optional<geo::BooleanOp> modeOf(const std::string& word) {
    if (word == "merge") {
        return geo::BooleanOp::merge;
    }
    if (word == "subtract") {
        return geo::BooleanOp::subtract;
    }
    if (word == "intersect") {
        return geo::BooleanOp::intersect;
    }
    return std::nullopt;
}

// The tool `input.inputs[index]` made again by its own step with the values for "through": long
// enough to pass the target on both sides. Its faces are named as the faces of the short tool
// are, so the walls of the hole keep their names. nullopt when it cannot be longer.
std::optional<geo::NamedSolid> longerTool(const ExecuteInput& input, std::size_t index,
                                          double targetSize, const ExecuteContext& context) {
    if (index >= input.producers.size()) {
        return std::nullopt;
    }
    const InputStep& step = input.producers[index];
    if (step.feature == nullptr || step.type == nullptr) {
        return std::nullopt;
    }
    const double toolSize = context.kernel.bounds(input.inputs[index].solid).diagonal();
    const auto longer = step.type->throughParams(step.params, targetSize + toolSize);
    if (!longer) {
        return std::nullopt;
    }
    const std::vector<FeatureOutput> none;
    FeatureResult made = step.type->execute({*step.feature, *longer, none}, context);
    if (made.outputs.empty()) {
        return std::nullopt;
    }
    geo::NamedSolid result;
    result.solid = std::move(made.outputs.front().solid);
    for (const geo::NamedFace& face : made.outputs.front().faces) {
        result.faces.push_back({face.faceId, elementName(*step.feature, face.role)});
    }
    return result;
}

class CombineType final : public FeatureType {
public:
    std::string_view id() const override { return "combine"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override { return {}; }

    std::vector<TextFieldSpec> texts() const override {
        // `auto`: "yes" for a step the program keeps up to date for a hole (ADR-0011).
        return {{"mode", "merge"}, {"auto", "no"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams&) const override {
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const std::optional<geo::BooleanOp> mode = modeOf(textOf(input.feature, "mode", "merge"));
        if (!mode || input.inputs.size() < 2) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "combine: needs a known mode, a target and a tool");
        }
        const auto named = [](const FeatureOutput& body) {
            return geo::NamedSolid{body.solid, body.faces};
        };
        std::vector<geo::NamedSolid> tools;
        const bool forHoles = textOf(input.feature, "auto", "no") == "yes";
        const double targetSize = context.kernel.bounds(input.inputs.front().solid).diagonal();
        for (std::size_t i = 1; i < input.inputs.size(); ++i) {
            // A hole goes all the way through (rule 13): its step makes the shape again, longer.
            if (forHoles && *mode == geo::BooleanOp::subtract) {
                if (auto longer = longerTool(input, i, targetSize, context)) {
                    tools.push_back(std::move(*longer));
                    continue;
                }
            }
            tools.push_back(named(input.inputs[i]));
        }
        geo::NamedSolid made = context.kernel.boolean(*mode, named(input.inputs.front()), tools);
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    Description describe(const ResolvedParams&) const override { return {"combine.merge", {}}; }

    Description describeStep(const Feature& feature, const ResolvedParams&) const override {
        const std::string mode = textOf(feature, "mode", "merge");
        const double tools = feature.inputs.empty() ? 0.0 : double(feature.inputs.size() - 1);
        return {"combine." + mode, {Quantity{tools, {}}}};
    }

    bool passesOnNames() const override { return true; }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeCombineType() {
    return std::make_unique<CombineType>();
}
} // namespace detail

} // namespace fission::features
