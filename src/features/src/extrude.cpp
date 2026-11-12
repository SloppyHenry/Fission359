// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pulling one region of a sketch out of its plane into a new body (AP-2.3b1, "Ziehen"). The step
// names the sketch step (word "sketch", an id as text) and the region (word "region", as
// `SketchRegionInfo::id`); the recompute passes the sketches of the earlier steps and puts the
// sketch into the cache key, so a changed sketch is calculated again.

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

class ExtrudeType final : public FeatureType {
public:
    std::string_view id() const override { return "extrude"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        std::vector<ParamSpec> own{{"distance", QuantityKind::length, "10"},
                                   {"taper", QuantityKind::angle, "0"}};
        // Where the person pointed inside the region, in sketch coordinates (u, v). Set when the
        // step is made; the person does not edit them (they are not shown).
        own.push_back({"insideU", QuantityKind::length, "0"});
        own.push_back({"insideV", QuantityKind::length, "0"});
        // Where the grip stands: a point on the face and the way it looks out (all 0 = no grip).
        // Set when the step is made; the person does not edit them (they are not shown).
        for (const char* name : {"anchorX", "anchorY", "anchorZ"}) {
            own.push_back({name, QuantityKind::length, "0"});
        }
        for (const char* name : {"anchorDx", "anchorDy", "anchorDz"}) {
            own.push_back({name, QuantityKind::plain, "0"});
        }
        return own;
    }

    std::vector<TextFieldSpec> texts() const override {
        // "sketch": the id of the sketch step; "region": the region's id ("L1+L2+L3+L4");
        // "extent": "distance" or "symmetric".
        return {{"sketch", ""}, {"region", ""}, {"extent", "distance"}, {"mode", "new"}};
    }

    std::optional<FeatureId> readsSketch(const Feature& feature) const override {
        const std::string text = textOf(feature, "sketch", "");
        if (text.empty()) {
            return std::nullopt;
        }
        return FeatureId{text};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const double distance = valueOf(params, "distance");
        if (!std::isfinite(distance) || distance == 0.0) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "distance";
            return issue;
        }
        const double taper = valueOf(params, "taper");
        if (!std::isfinite(taper) || taper < 0.0 || taper >= 89.0) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "taper";
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
                                         ? "extrude: needs one body to join or cut, or none"
                                         : "extrude: unknown mode '" + mode + "'",
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
                                     "extrude: the sketch is not there", "sketch");
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
                                     "extrude: the region is not there any more", "region");
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
                                         "extrude: no point inside the region", "region");
            }
        }

        const std::string extentWord = textOf(input.feature, "extent", "distance");
        std::optional<geo::ExtrudeExtent> extent;
        if (extentWord == "distance") {
            extent = geo::ExtrudeExtent::distance;
        } else if (extentWord == "symmetric") {
            extent = geo::ExtrudeExtent::symmetric;
        } else {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "extrude: unknown extent '" + extentWord + "'", "extent");
        }

        const SketchPlane& plane = *sketch->plane;
        const geo::SketchFrame frame{plane.origin(), plane.uAxis(), plane.vAxis()};
        const geo::ExtrudeSpec spec{
            valueOf(input.params, "distance"), *extent, valueOf(input.params, "taper"), {}};
        geo::NamedSolid made =
            context.kernel.extrudeRegion(built.curves, {inside.u, inside.v}, frame, spec);

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
                                         "extrude: the cut does not touch the body");
            }
        }
        geo::NamedSolid joined = context.kernel.boolean(
            op, geo::NamedSolid{target.solid, target.faces}, {std::move(made)}, 0.0);
        result.outputs.push_back({input.feature.outputs.at(0), std::move(joined.solid),
                                  std::move(joined.faces), std::move(joined.history)});
        return result;
    }

    // A new body, or joined to / cut from the one input body (AP-2.3b3).
    bool passesOnNames() const override { return true; }

    Description describe(const ResolvedParams& params) const override {
        return {"extrude", {params.at("distance")}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        const std::string mode = textOf(feature, "mode", "new");
        return {mode == "new" ? std::string("extrude") : "extrude." + mode,
                {params.at("distance")}};
    }

    // The grip stands where the person pointed inside the region, moved out by the distance;
    // dragging it along the way the body is pulled out changes the distance (rule 1).
    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const geo::Point3d way{valueOf(params, "anchorDx"), valueOf(params, "anchorDy"),
                               valueOf(params, "anchorDz")};
        const double length = std::hypot(way.x, way.y, way.z);
        if (!(length > 0.5)) {
            return {};
        }
        const double distance = valueOf(params, "distance");
        const geo::Point3d unit{way.x / length, way.y / length, way.z / length};
        const geo::Point3d spot{valueOf(params, "anchorX"), valueOf(params, "anchorY"),
                                valueOf(params, "anchorZ")};
        return {makeHandle(
            "distance", HandleKind::distance,
            {spot.x + unit.x * distance, spot.y + unit.y * distance, spot.z + unit.z * distance},
            unit, distance)};
    }

    std::vector<std::string> sizeParameters() const override { return {"distance"}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeExtrudeType() {
    return std::make_unique<ExtrudeType>();
}
} // namespace detail

} // namespace fission::features
