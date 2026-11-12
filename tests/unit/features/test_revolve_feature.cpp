// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The revolve step (AP-2.3c): turns one region of an earlier sketch step about an axis in the
// sketch plane into a new body, reads the sketch through the recompute, and is calculated again
// when the sketch changes. The sketch here is a rectangle u 10..20, v 0..30, turned about the
// v-axis through the origin (the default axis), which makes a tube.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/core/resolve.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;

namespace {

constexpr double kPi = std::numbers::pi;

struct RevolveWorld {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    History history{Document(21)};

    RevolveWorld() { fission::features::registerBuiltinFeatures(registry); }

    FeatureId addSketch() {
        FeatureDraft draft = defaultDraft(*registry.find("sketch"));
        draft.texts["basePlane"] = "xy";
        return *history.addFeature(std::move(draft));
    }

    // Four lines L1..L4 around the rectangle with the corners (10, 0) and (20, 30).
    void rectangle(FeatureId step) {
        const std::vector<std::vector<SketchPoint>> sides = {
            {{10, 0}, {20, 0}},
            {{20, 0}, {20, 30}},
            {{20, 30}, {10, 30}},
            {{10, 30}, {10, 0}},
        };
        for (const std::vector<SketchPoint>& side : sides) {
            history.execute(
                std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line, side));
        }
    }

    FeatureId addRevolve(FeatureId sketch, const std::string& region) {
        FeatureDraft draft = defaultDraft(*registry.find("revolve"));
        draft.texts["sketch"] = sketch.value;
        draft.texts["region"] = region;
        draft.params.at("angle").expression = "360";
        draft.params.at("insideU").expression = "15";
        draft.params.at("insideV").expression = "15";
        return *history.addFeature(std::move(draft));
    }

    RecomputeResult calculate() {
        const auto result = recompute(history.document(), registry, *kernel, cache);
        REQUIRE(result.has_value());
        return *result;
    }

    BodyId onlyBody() const {
        REQUIRE(history.document().bodies().size() == 1);
        return history.document().bodies().front().id;
    }
};

bool hasFaceEndingWith(const RecomputeResult& result, const BodyId& body,
                       const std::string& ending) {
    const auto found = result.bodies.find(body);
    if (found == result.bodies.end()) {
        return false;
    }
    for (const BodyFace& face : found->second.faces) {
        if (face.name.ends_with(ending)) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("revolve: a rectangle region of a sketch is turned into a tube with named faces",
          "[revolve][feature]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    const RecomputeResult result = world.calculate();
    REQUIRE(result.features.size() == 2);
    CHECK(result.state(sketch)->status == FeatureStatus::ok);
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    CHECK(state->description->key == "revolve");
    const BodyId body = world.onlyBody();
    REQUIRE(result.bodies.contains(body));
    // A full turn about the v-axis: pi * (20^2 - 10^2) * 30.
    CHECK(world.kernel->measure(result.bodies.at(body).solid).volume ==
          Approx(kPi * (20.0 * 20.0 - 10.0 * 10.0) * 30.0).epsilon(1e-9));
    for (const char* face : {"side[L1]", "side[L2]", "side[L3]", "side[L4]"}) {
        CHECK(hasFaceEndingWith(result, body, face));
    }
}

TEST_CASE("revolve: a half turn has two caps and half the volume", "[revolve][feature]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    REQUIRE(world.history.execute(makeSetFeatureValue(turn, "angle", "180")));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    CHECK(state->description->key == "revolve");
    const BodyId body = world.onlyBody();
    CHECK(world.kernel->measure(result.bodies.at(body).solid).volume ==
          Approx(kPi * (20.0 * 20.0 - 10.0 * 10.0) * 30.0 / 2.0).epsilon(1e-9));
    CHECK(hasFaceEndingWith(result, body, "capStart"));
    CHECK(hasFaceEndingWith(result, body, "capEnd"));
}

TEST_CASE("revolve: an angle out of range is an error on the step", "[revolve][feature]") {
    for (const char* angle : {"0", "400", "-90"}) {
        RevolveWorld world;
        const FeatureId sketch = world.addSketch();
        world.rectangle(sketch);
        const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
        REQUIRE(world.history.execute(makeSetFeatureValue(turn, "angle", angle)));
        const RecomputeResult result = world.calculate();
        const FeatureState* state = result.state(turn);
        REQUIRE(state != nullptr);
        INFO("angle = " << angle);
        CHECK(state->status != FeatureStatus::ok);
        REQUIRE(state->issue.has_value());
        CHECK(state->issue->code == FeatureIssueCode::parameterOutOfRange);
        CHECK(state->issue->subject == "angle");
        CHECK(result.bodies.empty());
    }
}

TEST_CASE("revolve: a zero axis direction is an error on the step", "[revolve][feature]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    REQUIRE(world.history.execute(makeSetFeatureValue(turn, "axisDx", "0")));
    REQUIRE(world.history.execute(makeSetFeatureValue(turn, "axisDy", "0")));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status != FeatureStatus::ok);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::parameterOutOfRange);
    CHECK(state->issue->subject == "axisDx");
}

TEST_CASE("revolve: joining without a body is an error", "[revolve][feature]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    FeatureDraft draft = defaultDraft(*world.registry.find("revolve"));
    draft.texts["sketch"] = sketch.value;
    draft.texts["region"] = "L1+L2+L3+L4";
    draft.texts["mode"] = "merge";
    draft.params.at("insideU").expression = "15";
    draft.params.at("insideV").expression = "15";
    const FeatureId turn = *world.history.addFeature(std::move(draft));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status != FeatureStatus::ok);
    CHECK(result.bodies.empty());
}

TEST_CASE("revolve: when the sketch changes, the body is calculated again", "[revolve][feature]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();
    const RecomputeResult first = world.calculate();
    CHECK(world.kernel->measure(first.bodies.at(body).solid).volume ==
          Approx(kPi * (20.0 * 20.0 - 10.0 * 10.0) * 30.0).epsilon(1e-9));

    // The rectangle becomes 10..30 wide: L1, L2 and L3 move.
    world.history.execute(makeMoveSketchPoints(
        sketch,
        {{"L1", {{10, 0}, {30, 0}}}, {"L2", {{30, 0}, {30, 30}}}, {"L3", {{30, 30}, {10, 30}}}}));
    const RecomputeResult wider = world.calculate();
    CHECK(wider.state(turn)->status == FeatureStatus::ok);
    CHECK(wider.executed == 2); // the sketch and the revolve were calculated again
    CHECK(world.kernel->measure(wider.bodies.at(body).solid).volume ==
          Approx(kPi * (30.0 * 30.0 - 10.0 * 10.0) * 30.0).epsilon(1e-9));
}
