// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Turning one region of a sketch about an axis in the plane of the sketch into a body (AP-2.3c,
// "Drehkörper"). The step names the sketch step (word "sketch", an id as text) and the region
// (word "region", as `SketchRegionInfo::id`); the recompute passes the sketches of the earlier
// steps and puts the sketch into the cache key, so a changed sketch is calculated again. The axis
// is given in sketch coordinates: a point on it ("axisU", "axisV") and its direction ("axisDx",
// "axisDy"); the default is the v-axis through the sketch origin.

#include <cmath>
#include <memory>
#include <string>

#include "fission/features/builtin.h"
#include "fission/features/sketch_regions.h"
#include "fission/geo/error.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

class RevolveType final : public FeatureType {
public:
    std::string_view id() const override { return "revolve"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        std::vector<ParamSpec> own{{"angle", QuantityKind::angle, "360"}};
        // Where the person pointed inside the region, in sketch coordinates (u, v). Set when the
        // step is made; the person does not edit them (they are not shown).
        own.push_back({"insideU", QuantityKind::length, "0"});
        own.push_back({"insideV", QuantityKind::length, "0"});
        // The axis of the turn, in sketch coordinates: a point on it and its direction.
        own.push_back({"axisU", QuantityKind::length, "0"});
        own.push_back({"axisV", QuantityKind::length, "0"});
        own.push_back({"axisDx", QuantityKind::plain, "0"});
        own.push_back({"axisDy", QuantityKind::plain, "1"});
        return own;
    }

    std::vector<TextFieldSpec> texts() const override {
        // "sketch": the id of the sketch step; "region": the region's id ("L1+L2+L3+L4").
        return {{"sketch", ""}, {"region", ""}, {"mode", "new"}};
    }

    std::optional<FeatureId> readsSketch(const Feature& feature) const override {
        const std::string text = textOf(feature, "sketch", "");
        if (text.empty()) {
            return std::nullopt;
        }
        return FeatureId{text};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const double angle = valueOf(params, "angle");
        if (!std::isfinite(angle) || angle <= 0.0 || angle > 360.0) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "angle";
            return issue;
        }
        const double axisDx = valueOf(params, "axisDx");
        const double axisDy = valueOf(params, "axisDy");
        if (!std::isfinite(axisDx) || !std::isfinite(axisDy) || (axisDx == 0.0 && axisDy == 0.0)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "axisDx";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const std::string mode = textOf(input.feature, "mode", "new");
        if (mode == "new" ? !input.inputs.empty()
                          : (mode != "merge" && mode != "subtract") || input.inputs.size() != 1) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     mode == "new" || mode == "merge" || mode == "subtract"
                                         ? "revolve: needs one body to join or cut, or none"
                                         : "revolve: unknown mode '" + mode + "'",
                                     "mode");
        }
        const SketchResult* sketch = nullptr;
        if (input.sketches != nullptr) {
            const auto found = input.sketches->find(FeatureId{textOf(input.feature, "sketch", "")});
            if (found != input.sketches->end()) {
                sketch = &found->second;
            }
        }
        if (sketch == nullptr || !sketch->plane) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "revolve: the sketch is not there", "sketch");
        }
        const std::vector<SketchElement>& elements = sketch->data.elements;

        const SketchCurves built = sketchCurvesOf(elements);
        const SketchRegionSet regions = computeSketchRegions(context.kernel, elements);
        const std::string regionId = textOf(input.feature, "region", "");
        const SketchRegionInfo* region = nullptr;
        for (const SketchRegionInfo& info : regions.regions) {
            if (info.id == regionId) {
                region = &info;
                break;
            }
        }
        if (region == nullptr) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "revolve: the region is not there any more", "region");
        }

        // A point inside the region: where the step was made, or the middle of the outer
        // boundary when that still lies inside; never a guess.
        SketchPoint inside{valueOf(input.params, "insideU"), valueOf(input.params, "insideV")};
        if (!regionContains(region->shape, inside)) {
            const std::vector<geo::Point2d>& polygon = region->shape.outer.polygon;
            geo::Point2d middle{};
            for (const geo::Point2d& point : polygon) {
                middle.x += point.x;
                middle.y += point.y;
            }
            if (!polygon.empty()) {
                middle.x /= static_cast<double>(polygon.size());
                middle.y /= static_cast<double>(polygon.size());
            }
            const SketchPoint fallback{middle.x, middle.y};
            if (regionContains(region->shape, fallback)) {
                inside = fallback;
            } else {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "revolve: no point inside the region", "region");
            }
        }

        const SketchPlane& plane = *sketch->plane;
        const geo::SketchFrame frame{plane.origin(), plane.uAxis(), plane.vAxis()};
        const geo::RevolveSpec spec{
            {valueOf(input.params, "axisU"), valueOf(input.params, "axisV")},
            {valueOf(input.params, "axisDx"), valueOf(input.params, "axisDy")},
            valueOf(input.params, "angle")};
        geo::NamedSolid made =
            context.kernel.revolveRegion(built.curves, {inside.u, inside.v}, frame, spec);

        // The kernel names the sides after the curve index ("side[c3]", "side[c3:2]"); the
        // element ids are the stable names ("side[L3]", "side[L3:2]").
        for (geo::NamedFace& face : made.faces) {
            const std::string& role = face.role;
            if (!role.starts_with("side[c")) {
                continue;
            }
            std::size_t end = 6;
            while (end < role.size() && role[end] >= '0' && role[end] <= '9') {
                ++end;
            }
            if (end > 6) {
                const std::size_t index = std::stoul(role.substr(6, end - 6));
                if (index < built.ids.size()) {
                    face.role = "side[" + built.ids[index] + role.substr(end);
                }
            }
        }

        // The step passes on the names of a body it joins to or cuts from, so it gives its own
        // faces their full names itself.
        for (geo::NamedFace& face : made.faces) {
            face.role = elementName(input.feature, face.role);
        }

        FeatureResult result;
        if (mode == "new") {
            result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                                      std::move(made.faces), std::move(made.history)});
            return result;
        }
        const FeatureOutput& target = input.inputs.front();
        const geo::BooleanOp op =
            mode == "merge" ? geo::BooleanOp::merge : geo::BooleanOp::subtract;
        if (op == geo::BooleanOp::subtract) {
            const geo::Bounds body = context.kernel.bounds(target.solid);
            const geo::Bounds cut = context.kernel.bounds(made.solid);
            const bool apart = cut.min.x > body.max.x || cut.max.x < body.min.x ||
                               cut.min.y > body.max.y || cut.max.y < body.min.y ||
                               cut.min.z > body.max.z || cut.max.z < body.min.z;
            if (apart) {
                throw geo::GeometryError(geo::GeometryErrorCode::noOverlap,
                                         "revolve: the cut does not touch the body");
            }
        }
        geo::NamedSolid joined = context.kernel.boolean(
            op, geo::NamedSolid{target.solid, target.faces}, {std::move(made)}, 0.0);
        result.outputs.push_back({input.feature.outputs.at(0), std::move(joined.solid),
                                  std::move(joined.faces), std::move(joined.history)});
        return result;
    }

    // A new body, or joined to / cut from the one input body.
    bool passesOnNames() const override { return true; }

    Description describe(const ResolvedParams& params) const override {
        return {"revolve", {params.at("angle")}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        const std::string mode = textOf(feature, "mode", "new");
        return {mode == "new" ? std::string("revolve") : "revolve." + mode, {params.at("angle")}};
    }

    // No grip: the turn is an angle, and there is no straight way to drag one.
    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        (void)params;
        return {};
    }

    std::vector<std::string> sizeParameters() const override { return {"angle"}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeRevolveType() {
    return std::make_unique<RevolveType>();
}
} // namespace detail

} // namespace fission::features
