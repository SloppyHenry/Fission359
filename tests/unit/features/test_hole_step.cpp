// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The step "hole" (AP-2.7b2): a hole drilled into a flat face as a step of the recipe.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kPlate = 40.0 * 40.0 * 10.0;

struct Drilled {
    RefWorld world;
    core::FeatureId box;
    core::FeatureId step;
    core::RecomputeResult result;
};

std::string topOf(RefWorld& world, const core::RecomputeResult& result,
                  const core::FeatureId& box) {
    const std::string suffix = "/box/top";
    for (const core::NamedLook& face : world.looksOf(result, box).faces) {
        if (face.name.size() > suffix.size() &&
            face.name.compare(face.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return face.name;
        }
    }
    FAIL("no top face");
    return {};
}

// A 40 x 40 x 10 plate with a hole put into its top face.
void drill(Drilled& d, std::map<std::string, std::string> values) {
    d.box = d.world.add(
        "box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}, {"y", "0"}});
    const core::RecomputeResult first = d.world.run();
    core::FeatureDraft draft = core::defaultDraft(*d.world.registry.find("hole"));
    draft.inputs = {d.world.bodyOf(d.box)};
    draft.refs["face"] = {
        d.world.pick(first, d.box, core::ElementKind::face, topOf(d.world, first, d.box))};
    for (const auto& [name, expression] : values) {
        draft.params.at(name).expression = expression;
    }
    d.step = *d.world.document.addFeature(std::move(draft));
    d.result = d.world.run();
}

double volumeOf(Drilled& d) {
    const fission::geo::SolidReport report =
        d.world.kernel->measure(d.result.bodies.at(d.world.bodyOf(d.step)).solid);
    CHECK(report.valid);
    return report.volume;
}

} // namespace

TEST_CASE("hole: an M3 hole straight through the top", "[holes]") {
    Drilled d;
    drill(d, {{"diameter", "3.4"}, {"depth", "12"}});
    REQUIRE(d.result.state(d.step) != nullptr);
    CHECK(d.result.state(d.step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(d) == Approx(kPlate - kPi * 1.7 * 1.7 * 10.0).epsilon(1e-6));
}

TEST_CASE("hole: a blind hole is as deep as asked", "[holes]") {
    Drilled d;
    drill(d, {{"diameter", "3.4"}, {"depth", "4"}});
    REQUIRE(d.result.state(d.step) != nullptr);
    CHECK(d.result.state(d.step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(d) == Approx(kPlate - kPi * 1.7 * 1.7 * 4.0).epsilon(1e-6));
}

TEST_CASE("hole: u moves the hole along the face", "[holes]") {
    Drilled d;
    drill(d, {{"diameter", "3.4"}, {"depth", "12"}, {"u", "5"}});
    REQUIRE(d.result.state(d.step) != nullptr);
    CHECK(d.result.state(d.step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(d) == Approx(kPlate - kPi * 1.7 * 1.7 * 10.0).epsilon(1e-6));
    // Moved the other way, the hole is still inside the plate and cuts the same.
    REQUIRE(d.world.document.setFeatureValue(d.step, "u", "-5"));
    d.result = d.world.run();
    CHECK(volumeOf(d) == Approx(kPlate - kPi * 1.7 * 1.7 * 10.0).epsilon(1e-6));
    // Beyond the edge (the plate is 20 mm to each side of the middle) only part of it cuts.
    REQUIRE(d.world.document.setFeatureValue(d.step, "u", "20"));
    d.result = d.world.run();
    CHECK(volumeOf(d) == Approx(kPlate - 0.5 * kPi * 1.7 * 1.7 * 10.0).epsilon(1e-6));
}

TEST_CASE("hole: a diameter of 0 is a problem that helps", "[holes]") {
    Drilled d;
    drill(d, {{"diameter", "0"}});
    REQUIRE(d.result.state(d.step) != nullptr);
    CHECK(d.result.state(d.step)->status == core::FeatureStatus::error);
    REQUIRE(d.result.state(d.step)->issue.has_value());
    CHECK(d.result.state(d.step)->issue->subject == "diameter");
}
