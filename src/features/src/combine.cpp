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
                const double reach =
                    targetSize + context.kernel.bounds(input.inputs[i].solid).diagonal();
                if (i < input.producers.size()) {
                    if (auto longer = throughTool(input.producers[i], reach, context)) {
                        tools.push_back(std::move(*longer));
                        continue;
                    }
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
