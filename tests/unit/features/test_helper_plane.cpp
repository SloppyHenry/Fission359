// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The step "helperPlane" (AP-2.12b): a plane without a body, offset from a base plane or from a
// flat face, passed on like the plane of an empty sketch.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;

namespace {

core::FeatureId addPlane(RefWorld& world, const char* distance,
                         std::vector<core::PersistentRef> face = {},
                         std::vector<core::BodyId> inputs = {}) {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find("helperPlane"));
    draft.params.at("distance").expression = distance;
    if (!face.empty()) {
        draft.refs["plane"] = std::move(face);
    }
    draft.inputs = std::move(inputs);
    return *world.document.addFeature(std::move(draft));
}

} // namespace

TEST_CASE("helper plane: offset from the ground plane, no body", "[helperplane][feature]") {
    RefWorld world;
    const core::FeatureId up = addPlane(world, "10");
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(up) != nullptr);
    CHECK(result.state(up)->status == core::FeatureStatus::ok);
    CHECK(result.bodies.empty());
    const auto plane = result.sketches.at(up).plane;
    REQUIRE(plane.has_value());
    CHECK(plane->origin().z == Approx(10.0));
    CHECK(plane->normal().z == Approx(1.0));

    REQUIRE(world.document.setFeatureValue(up, "distance", "-5"));
    const core::RecomputeResult down = world.run();
    CHECK(down.sketches.at(up).plane->origin().z == Approx(-5.0));
}

TEST_CASE("helper plane: from the top of a body, it moves with the body",
          "[helperplane][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    std::string top;
    for (const core::NamedLook& face : world.looksOf(first, box).faces) {
        if (face.name.ends_with("/box/top")) {
            top = face.name;
        }
    }
    REQUIRE_FALSE(top.empty());
    const core::FeatureId plane = addPlane(
        world, "10", {world.pick(first, box, core::ElementKind::face, top)}, {world.bodyOf(box)});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(plane) != nullptr);
    CHECK(result.state(plane)->status == core::FeatureStatus::ok);
    CHECK(result.sketches.at(plane).plane->origin().z == Approx(20.0));

    REQUIRE(world.document.setFeatureValue(box, "height", "20"));
    const core::RecomputeResult taller = world.run();
    CHECK(taller.sketches.at(plane).plane->origin().z == Approx(30.0));
}

TEST_CASE("helper plane: an unknown mode is a problem that helps", "[helperplane][feature]") {
    RefWorld world;
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find("helperPlane"));
    draft.texts["mode"] = "wobble";
    const core::FeatureId id = *world.document.addFeature(std::move(draft));
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(id) != nullptr);
    CHECK(result.state(id)->status == core::FeatureStatus::error);
}
