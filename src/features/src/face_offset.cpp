// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pushing and pulling a face (AP-2.4a). One body goes in, the face comes from the reference "face"
// (persistent, ADR-0003), one body comes out that carries the names of the input; the moved face
// keeps its name.

#include <cmath>
#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

class PushFaceType final : public FeatureType {
public:
    std::string_view id() const override { return "pushFace"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        // Along the way the face looks out of the body: positive makes the body larger there.
        std::vector<ParamSpec> own{{"distance", QuantityKind::length, "5"}};
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

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const double distance = valueOf(params, "distance");
        if (!std::isfinite(distance) || distance == 0.0) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "distance";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const auto picked = input.refs.find("face");
        if (input.inputs.size() != 1 || picked == input.refs.end() || picked->second.size() != 1) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "pushFace: needs one body and the reference 'face'");
        }
        const FeatureOutput& body = input.inputs.front();
        const geo::NamedSolid whole{body.solid, body.faces};
        geo::NamedSolid made = context.kernel.offsetFace(whole, picked->second.front().id,
                                                         valueOf(input.params, "distance"));
        FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                                  std::move(made.faces), std::move(made.history)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"pushFace", {params.at("distance")}};
    }

    bool passesOnNames() const override { return true; }

    // The grip sits on the face, moved by the distance; dragging it along the way the face looks
    // pulls the face out (rule 1).
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
std::unique_ptr<const core::FeatureType> makePushFaceType() {
    return std::make_unique<PushFaceType>();
}
} // namespace detail

} // namespace fission::features
