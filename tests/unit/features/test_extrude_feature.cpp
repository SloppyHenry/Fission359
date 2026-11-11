// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The extrude step (AP-2.3b1): pulls one region of an earlier sketch step into a new body, reads
// the sketch through the recompute, and is calculated again when the sketch changes.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;

namespace {

struct ExtrudeWorld {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    History history{Document(21)};

    ExtrudeWorld() { fission::features::registerBuiltinFeatures(registry); }

    FeatureId addSketch() {
        FeatureDraft draft = defaultDraft(*registry.find("sketch"));
        draft.texts["basePlane"] = "xy";
        return *history.addFeature(std::move(draft));
    }

    // Four lines L1..L4 around a width x depth rectangle.
    void rectangle(FeatureId step, double width, double depth) {
        const std::vector<std::vector<SketchPoint>> sides = {
            {{0, 0}, {width, 0}},
            {{width, 0}, {width, depth}},
            {{width, depth}, {0, depth}},
            {{0, depth}, {0, 0}},
        };
        for (const std::vector<SketchPoint>& side : sides) {
            history.execute(
                std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line, side));
        }
    }

    FeatureId addExtrude(FeatureId sketch, const std::string& region) {
        FeatureDraft draft = defaultDraft(*registry.find("extrude"));
        draft.texts["sketch"] = sketch.value;
        draft.texts["region"] = region;
        draft.params.at("distance").expression = "30";
        draft.params.at("insideU").expression = "30";
        draft.params.at("insideV").expression = "20";
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

TEST_CASE("extrude: a rectangle region of a sketch is pulled into a body with named faces",
          "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId pull = world.addExtrude(sketch, "L1+L2+L3+L4");
    const RecomputeResult result = world.calculate();
    REQUIRE(result.features.size() == 2);
    CHECK(result.state(sketch)->status == FeatureStatus::ok);
    const FeatureState* state = result.state(pull);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    CHECK(state->description->key == "extrude");
    const BodyId body = world.onlyBody();
    REQUIRE(result.bodies.contains(body));
    CHECK(world.kernel->measure(result.bodies.at(body).solid).volume ==
          Approx(72000.0).epsilon(1e-9));
    for (const char* face :
         {"side[L1]", "side[L2]", "side[L3]", "side[L4]", "capStart", "capEnd"}) {
        CHECK(hasFaceEndingWith(result, body, face));
    }
}

TEST_CASE("extrude: when the sketch changes, the body is calculated again", "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId pull = world.addExtrude(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();
    const RecomputeResult first = world.calculate();
    CHECK(world.kernel->measure(first.bodies.at(body).solid).volume ==
          Approx(72000.0).epsilon(1e-9));

    // The rectangle becomes 70 wide: L1, L2 and L3 move.
    world.history.execute(makeMoveSketchPoints(
        sketch,
        {{"L1", {{0, 0}, {70, 0}}}, {"L2", {{70, 0}, {70, 40}}}, {"L3", {{70, 40}, {0, 40}}}}));
    const RecomputeResult wider = world.calculate();
    CHECK(wider.state(pull)->status == FeatureStatus::ok);
    CHECK(wider.executed == 2); // the sketch and the extrude were calculated again
    CHECK(world.kernel->measure(wider.bodies.at(body).solid).volume ==
          Approx(84000.0).epsilon(1e-9));
}

TEST_CASE("extrude: a region that is not there any more is an error, never guessed",
          "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId pull = world.addExtrude(sketch, "L9");
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(pull);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::kernelRejected);
    CHECK(state->issue->subject == "region");
    CHECK(result.bodies.empty());
}

TEST_CASE("extrude: no sketch named is an error on the step", "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId pull = world.addExtrude(sketch, "L1+L2+L3+L4");
    world.history.execute(makeSetFeatureText(pull, "sketch", ""));
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(pull);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::kernelRejected);
    CHECK(state->issue->subject == "sketch");
    CHECK(result.bodies.empty());
}

TEST_CASE("extrude: an unchanged step is remembered", "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId pull = world.addExtrude(sketch, "L1+L2+L3+L4");
    const BodyId body = world.onlyBody();
    const RecomputeResult first = world.calculate();
    CHECK(first.executed == 2);

    const RecomputeResult again = world.calculate();
    CHECK(again.executed == 0);
    CHECK(again.remembered == 2);
    CHECK_FALSE(again.state(pull)->computed);
    CHECK(world.kernel->measure(again.bodies.at(body).solid).volume ==
          Approx(72000.0).epsilon(1e-9));
}

TEST_CASE("extrude: the grip stands where the region was picked and follows the distance",
          "[extrude][feature]") {
    const FeatureRegistry registry = [] {
        FeatureRegistry made;
        fission::features::registerBuiltinFeatures(made);
        return made;
    }();
    const FeatureType* type = registry.find("extrude");
    REQUIRE(type != nullptr);
    ResolvedParams params;
    const auto set = [&](const char* name, double value) { params[name] = {value, {}}; };
    set("distance", 30.0);
    set("anchorX", 0.0);
    set("anchorY", 0.0);
    set("anchorZ", 0.0);
    set("anchorDx", 0.0);
    set("anchorDy", 0.0);
    set("anchorDz", 1.0);
    const auto handles = type->handles(params);
    REQUIRE(handles.size() == 1);
    CHECK(handles[0].parameter == "distance");
    CHECK(handles[0].origin.x == Approx(0.0));
    CHECK(handles[0].origin.y == Approx(0.0));
    CHECK(handles[0].origin.z == Approx(30.0));
    CHECK(handles[0].direction.z == Approx(1.0));

    // Without a way out of the plane there is no grip.
    set("anchorDz", 0.0);
    CHECK(type->handles(params).empty());
}

namespace {

// A 100 x 100 x 20 box whose bottom lies on the sketch plane and covers the rectangle 60 x 40.
FeatureId addBox(ExtrudeWorld& world, const char* x) {
    FeatureDraft draft = defaultDraft(*world.registry.find("box"));
    draft.params.at("width").expression = "100";
    draft.params.at("depth").expression = "100";
    draft.params.at("height").expression = "20";
    draft.params.at("x").expression = x;
    draft.params.at("y").expression = "20";
    draft.params.at("z").expression = "0";
    return *world.history.addFeature(std::move(draft));
}

FeatureId addExtrudeOnto(ExtrudeWorld& world, FeatureId sketch, const FeatureId& box,
                         const char* mode, const char* distance) {
    FeatureDraft draft = defaultDraft(*world.registry.find("extrude"));
    draft.texts["sketch"] = sketch.value;
    draft.texts["region"] = "L1+L2+L3+L4";
    draft.texts["mode"] = mode;
    draft.params.at("distance").expression = distance;
    draft.params.at("insideU").expression = "30";
    draft.params.at("insideV").expression = "20";
    for (const Feature& feature : world.history.document().features()) {
        if (feature.id == box) {
            draft.inputs = feature.outputs;
        }
    }
    return *world.history.addFeature(std::move(draft));
}

double volumeOf(ExtrudeWorld& world, const RecomputeResult& result, const FeatureId& step) {
    for (const Feature& feature : world.history.document().features()) {
        if (feature.id == step) {
            return world.kernel->measure(result.bodies.at(feature.outputs.at(0)).solid).volume;
        }
    }
    FAIL("no such step");
    return 0.0;
}

} // namespace

TEST_CASE("extrude: pulled onto a body, the two become one (AP-2.3b3)", "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId box = addBox(world, "30");
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId pull = addExtrudeOnto(world, sketch, box, "merge", "30");
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(pull);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    CHECK(state->description->key == "extrude.merge");
    // 200000 for the box and 60 x 40 x 10 above it
    CHECK(volumeOf(world, result, pull) == Approx(224000.0).epsilon(1e-9));
}

TEST_CASE("extrude: cut into a body, the region is taken away (AP-2.3b3)", "[extrude][feature]") {
    ExtrudeWorld world;
    const FeatureId box = addBox(world, "30");
    const FeatureId sketch = world.addSketch();
    world.rectangle(sketch, 60, 40);
    const FeatureId cut = addExtrudeOnto(world, sketch, box, "subtract", "10");
    const RecomputeResult result = world.calculate();
    const FeatureState* state = result.state(cut);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    CHECK(state->description->key == "extrude.subtract");
    CHECK(volumeOf(world, result, cut) == Approx(176000.0).epsilon(1e-9));
}

TEST_CASE("extrude: a cut beside the body and a wrong number of bodies are errors (AP-2.3b3)",
          "[extrude][feature]") {
    SECTION("the cut does not touch the body") {
        ExtrudeWorld world;
        const FeatureId box = addBox(world, "500");
        const FeatureId sketch = world.addSketch();
        world.rectangle(sketch, 60, 40);
        const FeatureId cut = addExtrudeOnto(world, sketch, box, "subtract", "10");
        const RecomputeResult result = world.calculate();
        CHECK(result.state(cut)->status != FeatureStatus::ok);
    }
    SECTION("joining needs a body") {
        ExtrudeWorld world;
        const FeatureId sketch = world.addSketch();
        world.rectangle(sketch, 60, 40);
        FeatureDraft draft = defaultDraft(*world.registry.find("extrude"));
        draft.texts["sketch"] = sketch.value;
        draft.texts["region"] = "L1+L2+L3+L4";
        draft.texts["mode"] = "merge";
        draft.params.at("insideU").expression = "30";
        draft.params.at("insideV").expression = "20";
        const FeatureId pull = *world.history.addFeature(std::move(draft));
        const RecomputeResult result = world.calculate();
        CHECK(result.state(pull)->status != FeatureStatus::ok);
    }
    SECTION("an unknown mode") {
        ExtrudeWorld world;
        const FeatureId box = addBox(world, "30");
        const FeatureId sketch = world.addSketch();
        world.rectangle(sketch, 60, 40);
        const FeatureId pull = addExtrudeOnto(world, sketch, box, "melt", "10");
        const RecomputeResult result = world.calculate();
        CHECK(result.state(pull)->status != FeatureStatus::ok);
    }
}
