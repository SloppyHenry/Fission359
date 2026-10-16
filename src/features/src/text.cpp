// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/geo/primitives.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

constexpr const char* kDefaultText = "Text";
constexpr const char* kDefaultFont = "IBM Plex Sans";
constexpr const char* kDefaultStyle = "regular";

// Lettering, centred on (x, y), standing on the plane z. `size` is the height of a capital letter,
// `height` how far the letters stand up. The words (text, font, style) are typed words, not
// formulas.
// Whether the font has every letter, or exists at all, is for the kernel to say (execute).
class TextType final : public FeatureType {
public:
    std::string_view id() const override { return "text"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"size", QuantityKind::length, "10"},
                {"height", QuantityKind::length, "2"},
                {"x", QuantityKind::length, "0"},
                {"y", QuantityKind::length, "0"},
                {"z", QuantityKind::length, "0"}};
    }

    std::vector<TextFieldSpec> texts() const override {
        return {{"text", kDefaultText}, {"font", kDefaultFont}, {"style", kDefaultStyle}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return requirePositive(params, {"size", "height"});
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const std::optional<geo::FontStyle> style =
            geo::parseFontStyle(textOf(input.feature, "style", kDefaultStyle));
        if (!style) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "text: unknown style " + textOf(input.feature, "style", ""));
        }
        const geo::TextSpec spec{textOf(input.feature, "text", kDefaultText),
                                 textOf(input.feature, "font", kDefaultFont),
                                 valueOf(input.params, "size"), valueOf(input.params, "height"),
                                 *style};
        const geo::Point3d base{valueOf(input.params, "x"), valueOf(input.params, "y"),
                                valueOf(input.params, "z")};
        geo::NamedSolid made = context.kernel.makePrimitive(spec, base);
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"text", {params.at("size"), params.at("height")}};
    }

    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const double x = valueOf(params, "x");
        const double y = valueOf(params, "y");
        const double z = valueOf(params, "z");
        const double h = valueOf(params, "height");
        return {
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0},
            // At the top edge of capital letters: dragging towards the back makes the letters
            // taller.
            {"size",
             HandleKind::distance,
             "size",
             {x, y + valueOf(params, "size") / 2.0, z + h},
             {0, 1, 0},
             1.0},
        };
    }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeTextType() {
    return std::make_unique<TextType>();
}
} // namespace detail

} // namespace fission::features
