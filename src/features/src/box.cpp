// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A box standing on the plane z, centred on (x, y). Width runs along X, depth along Y. The four
// vertical edges can be rounded; the radius stays a true radius whatever the sides are (rule 11).
class BoxType final : public FeatureType {
public:
    std::string_view id() const override { return "box"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"width", QuantityKind::length, "20"},  {"depth", QuantityKind::length, "20"},
                {"height", QuantityKind::length, "20"}, {"cornerRadius", QuantityKind::length, "0"},
                {"x", QuantityKind::length, "0"},       {"y", QuantityKind::length, "0"},
                {"z", QuantityKind::length, "0"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        if (auto issue = requirePositive(params, {"width", "depth", "height"})) {
            return issue;
        }
        const double radius = valueOf(params, "cornerRadius");
        const double largest = std::min(valueOf(params, "width"), valueOf(params, "depth")) / 2.0;
        if (!std::isfinite(radius) || radius < 0.0 || radius > largest * (1.0 + 1e-12)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "cornerRadius";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const double width = valueOf(input.params, "width");
        const double depth = valueOf(input.params, "depth");
        const double height = valueOf(input.params, "height");
        const double cornerRadius = valueOf(input.params, "cornerRadius");
        const geo::Point3d corner{valueOf(input.params, "x") - width / 2.0,
                                  valueOf(input.params, "y") - depth / 2.0,
                                  valueOf(input.params, "z")};
        geo::NamedSolid made = context.kernel.makeBox(width, depth, height, corner, cornerRadius);
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"box", {params.at("width"), params.at("depth"), params.at("height")}};
    }

    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const double w = valueOf(params, "width");
        const double d = valueOf(params, "depth");
        const double h = valueOf(params, "height");
        const double r = valueOf(params, "cornerRadius");
        const double x = valueOf(params, "x");
        const double y = valueOf(params, "y");
        const double z = valueOf(params, "z");
        return {
            {"width",
             HandleKind::distance,
             "width",
             {x + w / 2.0, y, z + h / 2.0},
             {1, 0, 0},
             1.0,
             0.5},
            {"depth",
             HandleKind::distance,
             "depth",
             {x, y + d / 2.0, z + h / 2.0},
             {0, 1, 0},
             1.0,
             0.5},
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0, 1.0},
            // On the top edge of the front right corner, where the curve begins: dragging it
            // along the edge (to the left) makes the corner rounder, one to one.
            {"cornerRadius",
             HandleKind::distance,
             "cornerRadius",
             {x + w / 2.0 - r, y - d / 2.0, z + h},
             {-1, 0, 0},
             1.0,
             1.0},
        };
    }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeBoxType() {
    return std::make_unique<BoxType>();
}
} // namespace detail

} // namespace fission::features
