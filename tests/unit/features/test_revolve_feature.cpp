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
#include "fission/geo/sketch/planegcs_solver.h"

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

TEST_CASE("revolve: a named measure changes the solved sketch and so the turned body",
          "[revolve][feature][sketch]") {
    RevolveWorld world;
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    REQUIRE(world.history.execute(
        makeAddParameter(Parameter{"Breite", "10", QuantityKind::length, "", std::nullopt})));
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    for (const char* line : {"L1", "L3"}) {
        REQUIRE(world.history.execute(makeAddSketchRule(sketch, "horizontal", {line}, *solver)));
    }
    for (const char* line : {"L2", "L4"}) {
        REQUIRE(world.history.execute(makeAddSketchRule(sketch, "vertical", {line}, *solver)));
    }
    REQUIRE(world.history.execute(makeAddSketchRule(sketch, "fixed", {"L1.0"}, *solver)));
    for (const auto& pair : std::vector<std::vector<std::string>>{
             {"L1.1", "L2.0"}, {"L2.1", "L3.0"}, {"L3.1", "L4.0"}, {"L4.1", "L1.0"}}) {
        REQUIRE(world.history.execute(makeAddSketchRule(sketch, "coincident", pair, *solver)));
    }
    REQUIRE(world.history.execute(
        makeAddSketchMeasure(sketch, "length", {"L1"}, "Breite", 10.0, *solver)));
    world.addRevolve(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();

    const auto volumeOf = [&] {
        const auto result = recompute(world.history.document(), world.registry, *world.kernel,
                                      world.cache, {}, solver.get());
        REQUIRE(result.has_value());
        REQUIRE(result->bodies.contains(body));
        return world.kernel->measure(result->bodies.at(body).solid).volume;
    };
    CHECK(volumeOf() == Approx(kPi * (20.0 * 20.0 - 10.0 * 10.0) * 30.0).epsilon(1e-6));
    // Only the named measure changes: the cache must not hand back the old body.
    REQUIRE(world.history.execute(makeSetParameterExpression("Breite", "20")));
    CHECK(volumeOf() == Approx(kPi * (30.0 * 30.0 - 10.0 * 10.0) * 30.0).epsilon(1e-6));
}

// --- the axis can be chosen (AP-2.3d) ------------------------------------------------------

namespace {

double volumeOfTurn(RevolveWorld& world, const BodyId& body) {
    const RecomputeResult result = world.calculate();
    REQUIRE(result.bodies.contains(body));
    return world.kernel->measure(result.bodies.at(body).solid).volume;
}

} // namespace

TEST_CASE("revolve: the axis is the own line of the sketch", "[revolve][feature][axis]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    // L5 is the v-axis again, L6 stands at u = 5, L7 lies along the u-axis.
    for (const auto& points : std::vector<std::vector<SketchPoint>>{
             {{0, 0}, {0, 50}}, {{5, 0}, {5, 50}}, {{0, -5}, {50, -5}}}) {
        world.history.execute(
            std::make_unique<AddSketchElementCommand>(sketch, SketchElementKind::line, points));
    }
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();
    const double auto_ = volumeOfTurn(world, body);
    CHECK(auto_ == Approx(kPi * (400.0 - 100.0) * 30.0).epsilon(1e-9));

    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "line:L5")));
    CHECK(volumeOfTurn(world, body) == Approx(auto_).epsilon(1e-9));
    // The line at u = 5: a tube from the radius 5 to 15 (in the other direction: 15 to 5).
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "line:L6")));
    CHECK(volumeOfTurn(world, body) ==
          Approx(kPi * (15.0 * 15.0 - 5.0 * 5.0) * 30.0).epsilon(1e-9));
    // A line that runs along the u-axis and lies below the area: a tube from 5 to 35.
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "line:L7")));
    CHECK(volumeOfTurn(world, body) ==
          Approx(kPi * (35.0 * 35.0 - 5.0 * 5.0) * 10.0).epsilon(1e-9));
}

TEST_CASE("revolve: a main axis in the plane of the sketch is the axis",
          "[revolve][feature][axis]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch(); // plane xy: x is u, y is v
    world.rectangle(sketch);
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();
    // Around y: the same as the default axis.
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "y")));
    CHECK(volumeOfTurn(world, body) == Approx(kPi * (400.0 - 100.0) * 30.0).epsilon(1e-9));
    // Around x: the area (v 0..30, u 10..20) makes a disc of radius 30 and thickness 10.
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "x")));
    CHECK(volumeOfTurn(world, body) == Approx(kPi * 30.0 * 30.0 * 10.0).epsilon(1e-9));
    // z stands upright to the plane: not an axis of this sketch.
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "z")));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status != FeatureStatus::ok);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->subject == "axisOutOfPlane");
}

TEST_CASE("revolve: choosing the axis can be undone", "[revolve][feature][axis]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "x")));
    CHECK(volumeOfTurn(world, body) == Approx(kPi * 30.0 * 30.0 * 10.0).epsilon(1e-9));
    REQUIRE(world.history.undo());
    CHECK(volumeOfTurn(world, body) == Approx(kPi * (400.0 - 100.0) * 30.0).epsilon(1e-9));
    REQUIRE(world.history.redo());
    CHECK(volumeOfTurn(world, body) == Approx(kPi * 30.0 * 30.0 * 10.0).epsilon(1e-9));
}

TEST_CASE("revolve: a line that is gone is a message, not a guess", "[revolve][feature][axis]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch);
    world.history.execute(std::make_unique<AddSketchElementCommand>(
        sketch, SketchElementKind::line, std::vector<SketchPoint>{{0, 0}, {0, 50}}));
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "line:L5")));
    CHECK(world.calculate().state(turn)->status == FeatureStatus::ok);
    REQUIRE(world.history.execute(makeRemoveSketchElement(sketch, "L5")));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status != FeatureStatus::ok);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->subject == "axisMissing");
}

TEST_CASE("revolve: an area that crosses the axis is refused", "[revolve][feature][axis]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    // u -10..10, v 0..30: the default v-axis runs through the middle.
    for (const auto& side : std::vector<std::vector<SketchPoint>>{{{-10, 0}, {10, 0}},
                                                                  {{10, 0}, {10, 30}},
                                                                  {{10, 30}, {-10, 30}},
                                                                  {{-10, 30}, {-10, 0}}}) {
        world.history.execute(
            std::make_unique<AddSketchElementCommand>(sketch, SketchElementKind::line, side));
    }
    FeatureDraft draft = defaultDraft(*world.registry.find("revolve"));
    draft.texts["sketch"] = sketch.value;
    draft.texts["region"] = "L1+L2+L3+L4";
    draft.params.at("insideU").expression = "5";
    draft.params.at("insideV").expression = "15";
    const FeatureId turn = *world.history.addFeature(std::move(draft));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(turn);
    REQUIRE(state != nullptr);
    CHECK(state->status != FeatureStatus::ok);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->subject == "axis");
}

TEST_CASE("revolve: golden - a half turn about the x-axis (volume, area, box, faces, valid)",
          "[revolve][feature][axis][golden]") {
    RevolveWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch); // u 10..20, v 0..30
    const FeatureId turn = world.addRevolve(sketch, "L1+L2+L3+L4");
    REQUIRE(world.history.execute(makeSetFeatureText(turn, "axis", "x")));
    REQUIRE(world.history.execute(makeSetFeatureValue(turn, "angle", "180")));
    const RecomputeResult result = world.calculate();
    const BodyId body = world.onlyBody();
    const auto& solid = result.bodies.at(body).solid;
    const auto report = world.kernel->measure(solid);
    CHECK(report.valid);
    // A half disc of radius 30 and thickness 10.
    CHECK(report.volume == Approx(kPi * 30.0 * 30.0 * 10.0 / 2.0).epsilon(1e-9));
    // Curved side 2 pi r h / 2, the two half discs, the two caps (rectangles 10 x 30), the flat
    // side at v = 0 (a ring is not there: the area touches the axis, so it is one more flat face).
    CHECK(report.area ==
          Approx(kPi * 30.0 * 10.0 + 2.0 * (kPi * 30.0 * 30.0 / 2.0) + 2.0 * 10.0 * 30.0)
              .epsilon(1e-6));
    const auto box = world.kernel->bounds(solid);
    CHECK(box.min.x == Approx(10.0).margin(1e-6));
    CHECK(box.max.x == Approx(20.0).margin(1e-6));
    CHECK(box.max.y == Approx(30.0).margin(1e-6));
    CHECK(box.min.y == Approx(-30.0).margin(1e-6));
    CHECK(hasFaceEndingWith(result, body, "capStart"));
    CHECK(hasFaceEndingWith(result, body, "capEnd"));
}
