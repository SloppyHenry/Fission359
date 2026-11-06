// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Editing faces directly (AP-2.4b-e): stretching a body, removing faces, moving faces by a distance
// and turning a flat face about an edge. Each takes one body and the faces as persistent
// references (ADR-0003) and gives back one body that carries the names of its input.

#include <cmath>
#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

geo::Point3d anchorOf(const ResolvedParams& params, const char* x, const char* y, const char* z) {
    return {valueOf(params, x), valueOf(params, y), valueOf(params, z)};
}

// A body, its single "face" reference or the list "faces".
const FeatureOutput& bodyOf(const ExecuteInput& input, const char* type) {
    if (input.inputs.size() != 1) {
        throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                 std::string(type) + ": needs exactly one body");
    }
    return input.inputs.front();
}

std::vector<std::uint32_t> facesOf(const ExecuteInput& input, const char* slot, const char* type,
                                   std::size_t most) {
    const auto picked = input.refs.find(slot);
    if (picked == input.refs.end() || picked->second.empty() || picked->second.size() > most) {
        throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                 std::string(type) + ": needs the reference '" + slot + "'");
    }
    std::vector<std::uint32_t> ids;
    for (const ResolvedRef& ref : picked->second) {
        ids.push_back(ref.id);
    }
    return ids;
}

FeatureResult resultOf(const ExecuteInput& input, geo::NamedSolid made) {
    FeatureResult result;
    result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                              std::move(made.faces), std::move(made.history)});
    return result;
}

std::size_t countOf(const Feature& feature, const char* slot) {
    const auto found = feature.refs.find(slot);
    return found == feature.refs.end() ? 0 : found->second.size();
}

std::vector<ParamSpec> anchorParams(std::vector<ParamSpec> own, const char* prefix) {
    // Hidden numbers set when the step is made: where its grip stands.
    for (const char* axis : {"X", "Y", "Z"}) {
        own.push_back({std::string(prefix) + axis, QuantityKind::length, "0"});
    }
    for (const char* axis : {"Dx", "Dy", "Dz"}) {
        own.push_back({std::string(prefix) + axis, QuantityKind::plain, "0"});
    }
    return own;
}

std::optional<FeatureIssue> nonZero(const ResolvedParams& params, const char* name) {
    const double value = valueOf(params, name);
    if (!std::isfinite(value) || value == 0.0) {
        FeatureIssue issue;
        issue.code = FeatureIssueCode::parameterOutOfRange;
        issue.subject = name;
        return issue;
    }
    return std::nullopt;
}

// --- stretching ---------------------------------------------------------------------------

class StretchType final : public FeatureType {
public:
    std::string_view id() const override { return "stretch"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        // `distance` along the way the face looks out; `position` is the cutting place as a
        // coordinate along that way (set when the step is made, free to move).
        return anchorParams(
            {{"distance", QuantityKind::length, "10"}, {"position", QuantityKind::length, "0"}},
            "anchor");
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return nonZero(params, "distance");
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const FeatureOutput& body = bodyOf(input, "stretch");
        const std::uint32_t face = facesOf(input, "face", "stretch", 1).front();
        const geo::FaceAnchor looks = context.kernel.faceAnchor(body.solid, face);
        const geo::NamedSolid whole{body.solid, body.faces};
        return resultOf(input,
                        context.kernel.stretch(whole, looks.way, valueOf(input.params, "position"),
                                               valueOf(input.params, "distance")));
    }

    Description describe(const ResolvedParams& params) const override {
        return {"stretch", {params.at("distance")}};
    }

    bool passesOnNames() const override { return true; }

    // Two grips: the face (its distance) and the cutting plane (its place), both along the way.
    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const geo::Point3d way = anchorOf(params, "anchorDx", "anchorDy", "anchorDz");
        const double length = std::hypot(way.x, way.y, way.z);
        if (!(length > 0.5)) {
            return {};
        }
        const geo::Point3d unit{way.x / length, way.y / length, way.z / length};
        const geo::Point3d spot = anchorOf(params, "anchorX", "anchorY", "anchorZ");
        const double distance = valueOf(params, "distance");
        const double position = valueOf(params, "position");
        // The cutting plane's grip: on the line through the face's grip, at the place.
        const double onWay = spot.x * unit.x + spot.y * unit.y + spot.z * unit.z;
        std::vector<HandleSpec> out{
            makeHandle("distance", HandleKind::distance,
                       {spot.x + unit.x * distance, spot.y + unit.y * distance,
                        spot.z + unit.z * distance},
                       unit, distance),
            makeHandle("position", HandleKind::distance,
                       {spot.x + unit.x * (position - onWay), spot.y + unit.y * (position - onWay),
                        spot.z + unit.z * (position - onWay)},
                       unit, position)};
        markRole(out, 1, HandleRole::detail);
        return out;
    }

    std::vector<std::string> sizeParameters() const override { return {"distance"}; }
};

// --- removing faces -----------------------------------------------------------------------

class RemoveFacesType final : public FeatureType {
public:
    std::string_view id() const override { return "removeFaces"; }
    int version() const override { return 1; }
    std::vector<ParamSpec> parameters() const override { return {}; }
    std::optional<FeatureIssue> validate(const ResolvedParams&) const override {
        return std::nullopt;
    }
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const FeatureOutput& body = bodyOf(input, "removeFaces");
        const geo::NamedSolid whole{body.solid, body.faces};
        return resultOf(input, context.kernel.removeFaces(
                                   whole, facesOf(input, "faces", "removeFaces", std::size_t(-1))));
    }

    Description describe(const ResolvedParams&) const override { return {"removeFaces", {}}; }

    Description describeStep(const Feature& feature, const ResolvedParams&) const override {
        return {"removeFaces", {Quantity{double(countOf(feature, "faces")), {}}}};
    }

    bool passesOnNames() const override { return true; }
};

// --- moving several faces -----------------------------------------------------------------

class OffsetFacesType final : public FeatureType {
public:
    std::string_view id() const override { return "offsetFaces"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        return {{"distance", QuantityKind::length, "0.2"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return nonZero(params, "distance");
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const FeatureOutput& body = bodyOf(input, "offsetFaces");
        const geo::NamedSolid whole{body.solid, body.faces};
        return resultOf(input, context.kernel.offsetFaces(
                                   whole, facesOf(input, "faces", "offsetFaces", std::size_t(-1)),
                                   valueOf(input.params, "distance")));
    }

    Description describe(const ResolvedParams& params) const override {
        return {"offsetFaces", {params.at("distance")}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        return {"offsetFaces",
                {Quantity{double(countOf(feature, "faces")), {}}, params.at("distance")}};
    }

    bool passesOnNames() const override { return true; }
    // The distance is a fit: typed in the panel, there is no single place for a grip.
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
    std::vector<std::string> sizeParameters() const override { return {"distance"}; }
};

// --- turning a flat face ------------------------------------------------------------------

class TiltFaceType final : public FeatureType {
public:
    std::string_view id() const override { return "tiltFace"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        // The hinge: a point on it and its direction, set when the step is made (the edge the
        // face turns about); the person changes the angle.
        std::vector<ParamSpec> own{{"angle", QuantityKind::angle, "10"}};
        for (const char* name : {"pivotX", "pivotY", "pivotZ"}) {
            own.push_back({name, QuantityKind::length, "0"});
        }
        for (const char* name : {"axisX", "axisY", "axisZ"}) {
            own.push_back({name, QuantityKind::plain, "0"});
        }
        return own;
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        return nonZero(params, "angle");
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const FeatureOutput& body = bodyOf(input, "tiltFace");
        const geo::NamedSolid whole{body.solid, body.faces};
        return resultOf(
            input, context.kernel.tiltFace(whole, facesOf(input, "face", "tiltFace", 1).front(),
                                           anchorOf(input.params, "pivotX", "pivotY", "pivotZ"),
                                           anchorOf(input.params, "axisX", "axisY", "axisZ"),
                                           valueOf(input.params, "angle")));
    }

    Description describe(const ResolvedParams& params) const override {
        return {"tiltFace", {params.at("angle")}};
    }

    bool passesOnNames() const override { return true; }
    // An arc grip for turning does not exist yet: the angle is typed in the panel.
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeStretchType() {
    return std::make_unique<StretchType>();
}
std::unique_ptr<const core::FeatureType> makeRemoveFacesType() {
    return std::make_unique<RemoveFacesType>();
}
std::unique_ptr<const core::FeatureType> makeOffsetFacesType() {
    return std::make_unique<OffsetFacesType>();
}
std::unique_ptr<const core::FeatureType> makeTiltFaceType() {
    return std::make_unique<TiltFaceType>();
}
} // namespace detail

} // namespace fission::features
