// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Mirror step for bodies (AP-2.8c): the mirror image of one body at a plane. Three results
// (the word `mode`): "copy" keeps the body and adds its mirror image as a second body, "merge"
// joins both into one body, "only" keeps just the mirror image. The faces of the mirror image
// carry the name of the original face with `[mirror]` appended, so no name appears twice.

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "fission/features/builtin.h"
#include "fission/geo/pattern.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

class MirrorType final : public FeatureType {
public:
    std::string_view id() const override { return "mirror"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"planeX", QuantityKind::length, "0"}, {"planeY", QuantityKind::length, "0"},
                {"planeZ", QuantityKind::length, "0"}, {"normalX", QuantityKind::plain, "1"},
                {"normalY", QuantityKind::plain, "0"}, {"normalZ", QuantityKind::plain, "0"}};
    }

    std::vector<TextFieldSpec> texts() const override { return {{"mode", "copy"}}; }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const double x = valueOf(params, "normalX");
        const double y = valueOf(params, "normalY");
        const double z = valueOf(params, "normalZ");
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            std::hypot(x, y, z) < 1e-9) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "normalX"; // the plane has no direction
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        if (input.inputs.size() != 1) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "mirror: needs exactly one body");
        }
        const std::string mode = textOf(input.feature, "mode", "copy");
        if (mode != "copy" && mode != "merge" && mode != "only") {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput, "mirror: unknown mode",
                                     "mode");
        }
        // The step has one output body, or two when the original stays: the count is fixed when
        // the step is made, so a step whose mode does not fit is a problem, never a wrong body.
        const std::size_t wanted = mode == "copy" ? 2 : 1;
        if (input.feature.outputs.size() != wanted) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "mirror: the result kind does not fit the step", "mode");
        }

        const geo::NamedSolid original{input.inputs.front().solid, input.inputs.front().faces};
        const geo::Transform3 plane =
            geo::mirrorPlane({valueOf(input.params, "planeX"), valueOf(input.params, "planeY"),
                              valueOf(input.params, "planeZ")},
                             {valueOf(input.params, "normalX"), valueOf(input.params, "normalY"),
                              valueOf(input.params, "normalZ")});
        geo::NamedSolid image = context.kernel.transformed(original, plane);
        for (geo::NamedFace& face : image.faces) {
            face.role += "[mirror]";
        }
        if (context.kernel.measure(image.solid).volume <= 0.0) {
            throw geo::GeometryError(geo::GeometryErrorCode::kernelFailure,
                                     "mirror: the mirror image is inside out");
        }

        FeatureResult result;
        if (mode == "copy") {
            result.outputs.push_back(
                {input.feature.outputs.at(0), original.solid, original.faces, {}});
            result.outputs.push_back(
                {input.feature.outputs.at(1), std::move(image.solid), std::move(image.faces), {}});
        } else if (mode == "only") {
            result.outputs.push_back(
                {input.feature.outputs.at(0), std::move(image.solid), std::move(image.faces), {}});
        } else {
            geo::NamedSolid joined =
                context.kernel.boolean(geo::BooleanOp::merge, original, {image}, 0.0);
            result.outputs.push_back({input.feature.outputs.at(0), std::move(joined.solid),
                                      std::move(joined.faces), std::move(joined.history)});
        }
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"mirror",
                {params.at("planeX"), params.at("planeY"), params.at("planeZ"),
                 params.at("normalX"), params.at("normalY"), params.at("normalZ")}};
    }

    bool passesOnNames() const override { return true; }

    // The arrow stands on the plane and moves it along its axis; a slanted plane has none.
    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const double n[3] = {valueOf(params, "normalX"), valueOf(params, "normalY"),
                             valueOf(params, "normalZ")};
        int axis = 0;
        for (int i = 1; i < 3; ++i) {
            axis = std::abs(n[i]) > std::abs(n[axis]) ? i : axis;
        }
        const double length = std::hypot(n[0], n[1], n[2]);
        if (!(length > 1e-9) || std::abs(n[axis]) < length - 1e-9) {
            return {};
        }
        static const char* names[3] = {"planeX", "planeY", "planeZ"};
        const geo::Point3d origin{valueOf(params, "planeX"), valueOf(params, "planeY"),
                                  valueOf(params, "planeZ")};
        const geo::Point3d direction{axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0,
                                     axis == 2 ? 1.0 : 0.0};
        return {makeHandle(names[axis], HandleKind::distance, origin, direction, 0.0)};
    }

    std::vector<std::string> sizeParameters() const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeMirrorType() {
    return std::make_unique<MirrorType>();
}
} // namespace detail

} // namespace fission::features
