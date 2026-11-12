// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include "fission/core/sketch.h"
#include "fission/features/builtin.h"
#include "fission/geo/datum.h"
#include "fission/geo/error.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A helper plane (AP-2.12b): a step without a body, whose result is a plane without elements.
// It is passed on like the result of an empty sketch (`FeatureResult::sketch` with a set `plane`
// and empty `data`), so that later steps (sketch on helper plane) can read it.
class HelperPlaneType final : public FeatureType {
public:
    std::string_view id() const override { return "helperPlane"; }
    int version() const override { return 1; }
    int newBodies() const override { return 0; }
    bool carriesSketch() const override { return false; }

    std::vector<ParamSpec> parameters() const override {
        return {{"distance", QuantityKind::length, "10"}};
    }

    std::vector<TextFieldSpec> texts() const override {
        return {{"mode", "offset"}, {"basePlane", "xy"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const auto distanceIt = params.find("distance");
        if (distanceIt != params.end() && !std::isfinite(distanceIt->second.value)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "distance";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const std::string mode = textOf(input.feature, "mode", "offset");
        if (mode != "offset") {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "helperPlane: unknown mode '" + mode + "'", "mode");
        }
        const double distance = valueOf(input.params, "distance");

        // Determine the base plane: from a referenced face or from the basePlane word.
        std::optional<SketchPlane> basePlane;
        const auto onFace = input.feature.refs.find("plane");
        if (onFace != input.feature.refs.end() && !onFace->second.empty()) {
            const auto found = input.refs.find("plane");
            if (input.inputs.empty() || found == input.refs.end() || found->second.empty() ||
                found->second.front().kind != ElementKind::face) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "helperPlane: the face to build on is not there", "plane");
            }
            // Throws for a face that is not flat.
            const geo::FlatFace face =
                context.kernel.flatFace(input.inputs.front().solid, found->second.front().id);
            basePlane = planeOnFace(face.centre, face.normal, face.longestEdge);
        } else {
            const auto bp = basePlaneFromWord(textOf(input.feature, "basePlane", "xy"));
            if (!bp) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "helperPlane: unknown base plane", "basePlane");
            }
            basePlane = placedBasePlane(*bp, {0, 0, 0}, {0, 0, 0});
        }

        // Convert SketchPlane to DatumPlane, apply offset, convert back to SketchPlane.
        const SketchPlane& plane = *basePlane;
        geo::DatumPlane datum{plane.origin(), plane.normal(), plane.uAxis()};
        geo::DatumPlane offsetDatum = geo::offsetPlane(datum, distance);
        // vAxis = normal x uAxis
        const geo::Point3d vAxis = geo::Point3d{
            plane.normal().y * offsetDatum.uAxis.z - plane.normal().z * offsetDatum.uAxis.y,
            plane.normal().z * offsetDatum.uAxis.x - plane.normal().x * offsetDatum.uAxis.z,
            plane.normal().x * offsetDatum.uAxis.y - plane.normal().y * offsetDatum.uAxis.x};
        SketchPlane resultPlane(offsetDatum.origin, offsetDatum.uAxis, vAxis);

        FeatureResult result;
        result.sketch = SketchResult{resultPlane, {}};
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        const auto distanceIt = params.find("distance");
        if (distanceIt != params.end()) {
            return {"helperPlane", {distanceIt->second}};
        }
        return {"helperPlane", {}};
    }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }

    std::vector<std::string> sizeParameters() const override { return {"distance"}; }

    bool passesOnNames() const override { return false; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeHelperPlaneType() {
    return std::make_unique<HelperPlaneType>();
}
} // namespace detail

} // namespace fission::features
