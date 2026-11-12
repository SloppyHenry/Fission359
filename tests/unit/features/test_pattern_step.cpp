// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The step "pattern" (AP-2.8b): copies of a body in a row or a circle, merged with the original.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;

namespace {

core::FeatureId addPattern(RefWorld& world, const core::FeatureId& box, const char* mode,
                           std::map<std::string, std::string> values, const char* skip = "") {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find("pattern"));
    draft.inputs = {world.bodyOf(box)};
    draft.texts["mode"] = mode;
    draft.texts["skip"] = skip;
    for (const auto& [name, expression] : values) {
        draft.params.at(name).expression = expression;
    }
    return *world.document.addFeature(std::move(draft));
}

double volumeOf(RefWorld& world, const core::RecomputeResult& result, const core::FeatureId& step) {
    return world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid).volume;
}

} // namespace

TEST_CASE("pattern: a row of three, with one left out", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "3"}, {"spacing", "20"}});
    core::RecomputeResult result = world.run();
    REQUIRE(result.state(row) != nullptr);
    CHECK(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, row) == Approx(3000.0).epsilon(1e-9));

    REQUIRE(world.document.setFeatureText(row, "skip", "1"));
    result = world.run();
    CHECK(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, row) == Approx(2000.0).epsilon(1e-9));
}

TEST_CASE("pattern: a wrong count or mode is a problem that helps", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId none = addPattern(world, box, "linear", {{"count", "0"}});
    const core::FeatureId odd = addPattern(world, box, "wobble", {});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(none) != nullptr);
    CHECK(result.state(none)->status == core::FeatureStatus::error);
    REQUIRE(result.state(odd) != nullptr);
    CHECK(result.state(odd)->status != core::FeatureStatus::ok);
}
