// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Puts a part where the person moved it (ADR-0010 point 3, ADR-0011): a body made by a step that
// makes several bodies (a split) cannot be moved through that step, so each part gets a step of
// its own that moves it by the values x, y and z (mm).

#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

class PlaceType final : public FeatureType {
public:
    std::string_view id() const override { return "place"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"x", QuantityKind::length, "0"},
                {"y", QuantityKind::length, "0"},
                {"z", QuantityKind::length, "0"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams&) const override {
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        if (input.inputs.size() != 1) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "place: needs exactly one body");
        }
        const FeatureOutput& part = input.inputs.front();
        geo::NamedSolid moved = context.kernel.translate(
            {part.solid, part.faces},
            {valueOf(input.params, "x"), valueOf(input.params, "y"), valueOf(input.params, "z")});
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(moved.solid), std::move(moved.faces)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"place", {params.at("x"), params.at("y"), params.at("z")}};
    }

    bool passesOnNames() const override { return true; }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makePlaceType() {
    return std::make_unique<PlaceType>();
}
} // namespace detail

} // namespace fission::features
