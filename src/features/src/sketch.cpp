// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/sketch.h"

#include <cmath>
#include <memory>

#include "fission/features/builtin.h"
#include "fission/geo/error.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A sketch (ADR-0013): a plane and the elements drawn on it. It makes no body; its result is the
// plane and the elements (`SketchResult`), which the later steps that build bodies from sketches
// (AP-2.3) take their curves from. The plane is one of the three base planes (word `basePlane`),
// moved by x, y, z and turned by rotX, rotY, rotZ; or a flat face of a body (reference `plane`,
// the body is the step's input): the plane is worked out from the face every time, so it follows
// the body (AP-2.1b). A face that cannot be found again safely is not calculated at all (rule 6).
class SketchType final : public FeatureType {
public:
    std::string_view id() const override { return "sketch"; }
    int version() const override { return 1; }
    int newBodies() const override { return 0; }
    bool carriesSketch() const override { return true; }

    std::vector<ParamSpec> parameters() const override { return withPlace({}); }

    std::vector<TextFieldSpec> texts() const override {
        return {{"basePlane", std::string(basePlaneWord(BasePlane::xy))}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        for (const auto& [name, quantity] : params) {
            if (!std::isfinite(quantity.value)) {
                FeatureIssue issue;
                issue.code = FeatureIssueCode::parameterOutOfRange;
                issue.subject = name;
                return issue;
            }
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        SketchResult sketch;
        if (input.feature.sketch) {
            sketch.data = *input.feature.sketch;
        }
        if (!sketch.data.consistent()) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "sketch: the elements are not usable", "sketch");
        }
        const auto onFace = input.feature.refs.find("plane");
        if (onFace != input.feature.refs.end() && !onFace->second.empty()) {
            const auto found = input.refs.find("plane");
            if (input.inputs.empty() || found == input.refs.end() || found->second.empty() ||
                found->second.front().kind != ElementKind::face) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "sketch: the face to draw on is not there", "plane");
            }
            // Throws for a face that is not flat.
            const geo::FlatFace face =
                context.kernel.flatFace(input.inputs.front().solid, found->second.front().id);
            sketch.plane = planeOnFace(face.centre, face.normal, face.longestEdge);
        } else {
            const auto base = basePlaneFromWord(textOf(input.feature, "basePlane", "xy"));
            if (!base) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "sketch: unknown base plane", "basePlane");
            }
            sketch.plane = placedBasePlane(*base,
                                           {valueOf(input.params, "x"), valueOf(input.params, "y"),
                                            valueOf(input.params, "z")},
                                           turnOf(input.params));
        }
        FeatureResult result;
        result.sketch = std::move(sketch);
        return result;
    }

    Description describe(const ResolvedParams&) const override { return {"sketch", {}}; }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }

    // A sketch has no size to scale; its elements are edited in the sketch mode.
    std::vector<std::string> sizeParameters() const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeSketchType() {
    return std::make_unique<SketchType>();
}
} // namespace detail

} // namespace fission::features
