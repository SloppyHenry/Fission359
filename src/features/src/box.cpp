// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include "fission/features/builtin.h"
#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A box standing on the plane z, centred on (x, y). Width runs along X, depth along Y.
class BoxType final : public FeatureType {
public:
    std::string_view id() const override { return "box"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"width", QuantityKind::length, "20"}, {"depth", QuantityKind::length, "20"},
                {"height", QuantityKind::length, "20"}, {"x", QuantityKind::length, "0"},
                {"y", QuantityKind::length, "0"},       {"z", QuantityKind::length, "0"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return requirePositive(params, {"width", "depth", "height"});
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const double width = valueOf(input.params, "width");
        const double depth = valueOf(input.params, "depth");
        const double height = valueOf(input.params, "height");
        const geo::Point3d corner{valueOf(input.params, "x") - width / 2.0,
                                  valueOf(input.params, "y") - depth / 2.0,
                                  valueOf(input.params, "z")};
        geo::NamedSolid made = context.kernel.makeBox(width, depth, height, corner);
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"box",
                {params.at("width"), params.at("depth"), params.at("height")}};
    }

    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const double w = valueOf(params, "width");
        const double d = valueOf(params, "depth");
        const double h = valueOf(params, "height");
        const double x = valueOf(params, "x");
        const double y = valueOf(params, "y");
        const double z = valueOf(params, "z");
        return {
            {"width", HandleKind::distance, "width", {x + w / 2.0, y, z + h / 2.0}, {1, 0, 0}, 1.0},
            {"depth", HandleKind::distance, "depth", {x, y + d / 2.0, z + h / 2.0}, {0, 1, 0}, 1.0},
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0},
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
