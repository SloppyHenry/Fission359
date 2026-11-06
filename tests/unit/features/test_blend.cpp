// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The steps "fillet" and "chamfer" (AP-2.5): the edge comes from a persistent reference, the
// result keeps the names of the body, and a size that is too large tells the largest that works.

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

// The edge along z with the smallest x and y of the body `step` made, by its name.
std::string verticalEdge(RefWorld& world, const core::RecomputeResult& result,
                         const core::FeatureId& step) {
    const core::BodyLooks looks = world.looksOf(result, step);
    const core::NamedLook* best = nullptr;
    for (const core::NamedLook& edge : looks.edges) {
        if (std::abs(edge.look.direction.z) < 0.9) {
            continue;
        }
        if (best == nullptr || edge.look.centroid.x + edge.look.centroid.y <
                                   best->look.centroid.x + best->look.centroid.y) {
            best = &edge;
        }
    }
    REQUIRE(best != nullptr);
    return best->name;
}

core::FeatureId blend(RefWorld& world, const char* type, const core::FeatureId& target,
                      core::PersistentRef edge, const char* size) {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find(type));
    draft.inputs = {world.bodyOf(target)};
    draft.refs["edges"] = {std::move(edge)};
    draft.params.at(std::string(type) == "fillet" ? "radius" : "distance").expression = size;
    return *world.document.addFeature(std::move(draft));
}

} // namespace

TEST_CASE("fillet: the edge comes from a reference and the body keeps its names", "[blend]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const std::string name = verticalEdge(world, first, plate);
    const core::FeatureId round =
        blend(world, "fillet", plate, world.pick(first, plate, core::ElementKind::edge, name), "3");

    const core::RecomputeResult result = world.run();
    const core::FeatureState* state = result.state(round);
    REQUIRE(state != nullptr);
    CHECK(state->status == core::FeatureStatus::ok);
    const core::BodyResult& body = result.bodies.at(world.bodyOf(round));
    const fission::geo::SolidReport report = world.kernel->measure(body.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(16000.0 - 9.0 * (1.0 - kPi / 4.0) * 10.0).epsilon(1e-6));
    CHECK(report.faces == 7);

    bool top = false;
    bool made = false;
    for (const core::BodyFace& face : body.faces) {
        top = top || face.name == "F1/box/top";
        made = made || face.name.rfind("F2/fillet/round[", 0) == 0;
    }
    CHECK(top);
    CHECK(made);
}

TEST_CASE("fillet: a size that is too large names the largest that works", "[blend]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const std::string name = verticalEdge(world, first, plate);
    const core::FeatureId round = blend(
        world, "fillet", plate, world.pick(first, plate, core::ElementKind::edge, name), "80");

    const core::RecomputeResult result = world.run();
    const core::FeatureState* state = result.state(round);
    REQUIRE(state != nullptr);
    CHECK(state->status == core::FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == core::FeatureIssueCode::kernelRejected);
    CHECK(state->issue->subject == "radius");
    CHECK(state->issue->numbers.at("given") == Approx(80.0));
    REQUIRE(state->issue->numbers.count("maxValue") == 1);
    const double largest = state->issue->numbers.at("maxValue");
    CHECK(largest >= 0.1);
    CHECK(largest < 80.0);
    CHECK(state->issue->where.has_value());
    CHECK(state->issue->elements.size() == 1);

    // The number is good: with it the step works.
    REQUIRE(world.document.setFeatureValue(round, "radius", std::to_string(largest)));
    const core::RecomputeResult again = world.run();
    CHECK(again.state(round)->status == core::FeatureStatus::ok);
    CHECK(world.kernel->measure(again.bodies.at(world.bodyOf(round)).solid).valid);
}

TEST_CASE("chamfer: a bevel keeps the names and takes the volume of its triangle", "[blend]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const std::string name = verticalEdge(world, first, plate);
    const core::FeatureId bevel = blend(
        world, "chamfer", plate, world.pick(first, plate, core::ElementKind::edge, name), "2");

    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(bevel)->status == core::FeatureStatus::ok);
    const fission::geo::SolidReport report =
        world.kernel->measure(result.bodies.at(world.bodyOf(bevel)).solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(16000.0 - 0.5 * 2.0 * 2.0 * 10.0));
}

TEST_CASE("fillet: the rounding stays on its edge when the body grows", "[blend][naming]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    const std::string name = verticalEdge(world, first, plate);
    const core::FeatureId round =
        blend(world, "fillet", plate, world.pick(first, plate, core::ElementKind::edge, name), "3");
    REQUIRE(world.run().state(round)->status == core::FeatureStatus::ok);

    REQUIRE(world.document.setFeatureValue(plate, "height", "25"));
    const core::RecomputeResult taller = world.run();
    REQUIRE(taller.state(round)->status == core::FeatureStatus::ok);
    CHECK(world.kernel->measure(taller.bodies.at(world.bodyOf(round)).solid).volume ==
          Approx(40.0 * 40.0 * 25.0 - 9.0 * (1.0 - kPi / 4.0) * 25.0).epsilon(1e-6));
}
