// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A cylinder standing on the plane z, its axis along Z through (x, y).
class CylinderType final : public FeatureType {
public:
    std::string_view id() const override { return "cylinder"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"radius", QuantityKind::length, "10"},
                {"height", QuantityKind::length, "20"},
                {"x", QuantityKind::length, "0"},
                {"y", QuantityKind::length, "0"},
                {"z", QuantityKind::length, "0"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return requirePositive(params, {"radius", "height"});
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const geo::Point3d base{valueOf(input.params, "x"), valueOf(input.params, "y"),
                                valueOf(input.params, "z")};
        geo::NamedSolid made = context.kernel.makeCylinder(valueOf(input.params, "radius"),
                                                           valueOf(input.params, "height"), base);
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"cylinder", {params.at("radius"), params.at("height")}};
    }

    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const double r = valueOf(params, "radius");
        const double h = valueOf(params, "height");
        const double x = valueOf(params, "x");
        const double y = valueOf(params, "y");
        const double z = valueOf(params, "z");
        return {
            {"radius", HandleKind::radius, "radius", {x + r, y, z + h / 2.0}, {1, 0, 0}, 1.0},
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0},
        };
    }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeCylinderType() {
    return std::make_unique<CylinderType>();
}
} // namespace detail

} // namespace fission::features
