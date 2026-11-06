// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The step "pushFace" (AP-2.4a): the face comes from a persistent reference, the body keeps its
// names, the step follows when the body under it changes, and it has a grip on the face.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "fission/features/builtin.h"

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;

namespace {

std::string faceNamed(RefWorld& world, const core::RecomputeResult& result,
                      const core::FeatureId& step, const std::string& role) {
    for (const core::NamedLook& face : world.looksOf(result, step).faces) {
        if (face.name.size() > role.size() &&
            face.name.compare(face.name.size() - role.size(), role.size(), role) == 0) {
            return face.name;
        }
    }
    FAIL("no face with the role " << role);
    return {};
}

core::FeatureId pushFace(RefWorld& world, const core::FeatureId& target, core::PersistentRef face,
                         const char* distance) {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find("pushFace"));
    draft.inputs = {world.bodyOf(target)};
    draft.refs["face"] = {std::move(face)};
    draft.params.at("distance").expression = distance;
    return *world.document.addFeature(std::move(draft));
}

} // namespace

TEST_CASE("pushFace: a wall pulled out keeps the names of the body", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const core::FeatureId step = pushFace(
        world, plate,
        world.pick(first, plate, core::ElementKind::face, faceNamed(world, first, plate, "/right")),
        "5");

    const core::RecomputeResult result = world.run();
    const core::FeatureState* state = result.state(step);
    REQUIRE(state != nullptr);
    CHECK(state->status == core::FeatureStatus::ok);
    const core::BodyResult& body = result.bodies.at(world.bodyOf(step));
    const fission::geo::SolidReport report = world.kernel->measure(body.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(45.0 * 40.0 * 10.0).epsilon(1e-9));
    CHECK(report.faces == 6);
    bool right = false;
    for (const core::BodyFace& face : body.faces) {
        right = right || face.name == "F1/box/right";
    }
    CHECK(right); // the wall moved, it is still the same face
}

TEST_CASE("pushFace: the distance can be negative, zero is refused", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const core::PersistentRef top =
        world.pick(first, plate, core::ElementKind::face, faceNamed(world, first, plate, "/top"));
    const core::FeatureId step = pushFace(world, plate, top, "-4");
    const core::RecomputeResult result = world.run();
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid).volume ==
          Approx(40.0 * 40.0 * 6.0).epsilon(1e-9));

    REQUIRE(world.document.setFeatureValue(step, "distance", "0"));
    const core::RecomputeResult none = world.run();
    CHECK(none.state(step)->status == core::FeatureStatus::error);
}

TEST_CASE("pushFace: the step follows when the body under it changes", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const core::FeatureId step = pushFace(
        world, plate,
        world.pick(first, plate, core::ElementKind::face, faceNamed(world, first, plate, "/right")),
        "5");
    REQUIRE(world.document.setFeatureValue(plate, "width", "60"));
    const core::RecomputeResult result = world.run();
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid).volume ==
          Approx(65.0 * 40.0 * 10.0).epsilon(1e-9));
}

TEST_CASE("pushFace: the grip stands on the face and follows the distance", "[faces]") {
    const core::FeatureRegistry registry = [] {
        core::FeatureRegistry made;
        fission::features::registerBuiltinFeatures(made);
        return made;
    }();
    const core::FeatureType* type = registry.find("pushFace");
    REQUIRE(type != nullptr);
    core::ResolvedParams params;
    const auto set = [&](const char* name, double value) { params[name] = {value, {}}; };
    set("distance", 5.0);
    set("anchorX", 40.0);
    set("anchorY", 20.0);
    set("anchorZ", 5.0);
    set("anchorDx", 1.0);
    set("anchorDy", 0.0);
    set("anchorDz", 0.0);
    const auto handles = type->handles(params);
    REQUIRE(handles.size() == 1);
    CHECK(handles[0].parameter == "distance");
    CHECK(handles[0].origin.x == Approx(45.0));
    CHECK(handles[0].origin.y == Approx(20.0));
    CHECK(handles[0].direction.x == Approx(1.0));

    // Without an anchor there is no grip.
    set("anchorDx", 0.0);
    CHECK(type->handles(params).empty());
}
