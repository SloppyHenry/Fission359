// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pattern step for bodies (AP-2.8b): linear and circular patterns.

#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

std::optional<geo::Point3d> directionOf(const ResolvedParams& params, const char* prefix) {
    const double x = valueOf(params, (std::string(prefix) + "X").c_str());
    const double y = valueOf(params, (std::string(prefix) + "Y").c_str());
    const double z = valueOf(params, (std::string(prefix) + "Z").c_str());
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return std::nullopt;
    }
    return geo::Point3d{x, y, z};
}

std::optional<geo::Point3d> pointOf(const ResolvedParams& params, const char* prefix) {
    const double x = valueOf(params, (std::string(prefix) + "X").c_str());
    const double y = valueOf(params, (std::string(prefix) + "Y").c_str());
    const double z = valueOf(params, (std::string(prefix) + "Z").c_str());
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return std::nullopt;
    }
    return geo::Point3d{x, y, z};
}

// Parse a comma-separated list of integers from a string (e.g., "3,7,12").
// Non-integer values are silently ignored.
std::vector<int> parseSkipIndices(const std::string& text) {
    std::vector<int> indices;
    std::istringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        try {
            // Try to parse as integer, ignoring any trailing whitespace
            size_t pos = 0;
            const int value = std::stoi(token, &pos);
            if (pos == token.size()) {
                indices.push_back(value);
            }
        } catch (...) {
            // Ignore non-integer tokens
        }
    }
    return indices;
}

class PatternType final : public FeatureType {
public:
    std::string_view id() const override { return "pattern"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"count", QuantityKind::plain, "3"},  {"spacing", QuantityKind::length, "20"},
                {"dirX", QuantityKind::plain, "1"},   {"dirY", QuantityKind::plain, "0"},
                {"dirZ", QuantityKind::plain, "0"},   {"total", QuantityKind::angle, "360"},
                {"axisX", QuantityKind::length, "0"}, {"axisY", QuantityKind::length, "0"},
                {"axisZ", QuantityKind::length, "0"}, {"axisDx", QuantityKind::plain, "0"},
                {"axisDy", QuantityKind::plain, "0"}, {"axisDz", QuantityKind::plain, "1"}};
    }

    std::vector<TextFieldSpec> texts() const override { return {{"mode", "linear"}, {"skip", ""}}; }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const auto countIt = params.find("count");
        if (countIt == params.end()) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "count";
            return issue;
        }
        const double countValue = countIt->second.value;
        // Check that count is an integer-like finite value in [1, 10000]
        if (!std::isfinite(countValue) ||
            !(countValue >= 1.0 && countValue <= 10000.0 && std::floor(countValue) == countValue)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "count";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        if (input.inputs.size() != 1) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "pattern: needs exactly one body");
        }

        const std::string mode = textOf(input.feature, "mode", "linear");
        if (mode != "linear" && mode != "circular") {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput, "pattern: unknown mode",
                                     "mode");
        }

        const std::string skipText = textOf(input.feature, "skip", "");
        const std::vector<int> skipIndices = parseSkipIndices(skipText);

        const int count = static_cast<int>(std::round(valueOf(input.params, "count")));
        const geo::NamedSolid original{input.inputs.front().solid, input.inputs.front().faces};

        std::vector<geo::Transform3> transforms;
        if (mode == "linear") {
            const std::optional<geo::Point3d> dirOpt = directionOf(input.params, "dir");
            if (!dirOpt) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "pattern: direction not finite", "dir");
            }
            const double spacing = valueOf(input.params, "spacing");
            if (!std::isfinite(spacing)) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "pattern: spacing not finite", "spacing");
            }
            transforms = geo::linearPattern(*dirOpt, spacing, count);
        } else { // circular
            const std::optional<geo::Point3d> axisPointOpt = pointOf(input.params, "axis");
            const std::optional<geo::Point3d> axisDirOpt = directionOf(input.params, "axisD");
            if (!axisPointOpt || !axisDirOpt) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "pattern: axis not finite", "axis");
            }
            const double total = valueOf(input.params, "total");
            if (!std::isfinite(total)) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "pattern: total not finite", "total");
            }
            transforms = geo::circularPattern(*axisPointOpt, *axisDirOpt, count, total);
        }

        // Apply skip indices
        std::vector<geo::Transform3> effectiveTransforms =
            geo::withSkipped(transforms, skipIndices);

        // If no copies remain (only the original, which is already the input), return original
        if (effectiveTransforms.size() <= 1) {
            FeatureResult result;
            result.outputs.push_back(
                {input.feature.outputs.at(0), original.solid, original.faces, {}});
            return result;
        }

        // Create transformed copies (skip index 0 which is identity/original)
        std::vector<geo::NamedSolid> copies;
        for (size_t i = 1; i < effectiveTransforms.size(); ++i) {
            copies.push_back(context.kernel.transformed(original, effectiveTransforms[i]));
        }

        // Merge all copies with the original using boolean merge
        geo::NamedSolid result =
            context.kernel.boolean(geo::BooleanOp::merge, original, copies, 0.0);

        FeatureResult featureResult;
        featureResult.outputs.push_back({input.feature.outputs.at(0), std::move(result.solid),
                                         std::move(result.faces), std::move(result.history)});
        return featureResult;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"pattern", {params.at("count")}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        const std::string mode = textOf(feature, "mode", "linear");
        return {"pattern." + mode, {params.at("count")}};
    }

    bool passesOnNames() const override { return true; }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }

    std::vector<std::string> sizeParameters() const override { return {"count", "spacing"}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makePatternType() {
    return std::make_unique<PatternType>();
}
} // namespace detail

} // namespace fission::features
