// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The sketch step (AP-2.1a, ADR-0013): makes no body, its result is the plane and the elements,
// changes are seen by the recompute, and the step is registered like every other type.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "fission/core/body_looks.h"
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

struct SketchWorld {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    History history{Document(21)};

    SketchWorld() { fission::features::registerBuiltinFeatures(registry); }

    FeatureId addSketch(const std::string& basePlane = "xy") {
        FeatureDraft draft = defaultDraft(*registry.find("sketch"));
        draft.texts["basePlane"] = basePlane;
        return *history.addFeature(std::move(draft));
    }

    RecomputeResult calculate() {
        const auto result = recompute(history.document(), registry, *kernel, cache);
        REQUIRE(result.has_value());
        return *result;
    }
};

std::vector<SketchPoint> segment(double u1, double v1, double u2, double v2) {
    return {{u1, v1}, {u2, v2}};
}

} // namespace

TEST_CASE("sketch step: registered, makes no body, starts with an empty sketch", "[sketch]") {
    SketchWorld world;
    const FeatureType* type = world.registry.find("sketch");
    REQUIRE(type != nullptr);
    CHECK(type->newBodies() == 0);
    CHECK(type->carriesSketch());
    CHECK(type->handles({}).empty());
    const FeatureDraft draft = defaultDraft(*type);
    REQUIRE(draft.sketch.has_value());
    CHECK(draft.sketch->elements.empty());
    CHECK(draft.texts.at("basePlane") == "xy");
    CHECK(draft.newBodies == 0);
    CHECK_FALSE(world.registry.find("box")->carriesSketch());
    CHECK_FALSE(defaultDraft(*world.registry.find("box")).sketch.has_value());
}

TEST_CASE("sketch step: the recompute gives the plane and the elements, no body", "[sketch]") {
    SketchWorld world;
    const FeatureId step = world.addSketch("xz");
    world.history.execute(std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line,
                                                                    segment(0, 0, 30, 10)));
    const RecomputeResult result = world.calculate();
    CHECK(result.bodies.empty());
    REQUIRE(result.features.size() == 1);
    CHECK(result.features[0].status == FeatureStatus::ok);
    CHECK(result.features[0].description->key == "sketch");

    REQUIRE(result.sketches.contains(step));
    const SketchResult& sketch = result.sketches.at(step);
    REQUIRE(sketch.plane.has_value());
    CHECK(sketch.plane->normal().y == Approx(-1.0));
    CHECK(sketch.data.elements.size() == 1);
    CHECK(sketch.data.elements[0].id == "L1");
    const auto end = sketch.plane->toWorld(sketch.data.elements[0].points[1]);
    CHECK(end.x == Approx(30.0));
    CHECK(end.y == Approx(0.0).margin(1e-9));
    CHECK(end.z == Approx(10.0));
}

TEST_CASE("sketch step: place and turn move the plane", "[sketch]") {
    SketchWorld world;
    const FeatureId step = world.addSketch("xy");
    world.history.execute(makeSetFeatureValue(step, "z", "12"));
    world.history.execute(makeSetFeatureValue(step, "rotZ", "90"));
    const RecomputeResult result = world.calculate();
    const SketchPlane& plane = *result.sketches.at(step).plane;
    CHECK(plane.origin().z == Approx(12.0));
    CHECK(plane.uAxis().y == Approx(1.0));
    CHECK(plane.vAxis().x == Approx(-1.0));
}

TEST_CASE(
    "sketch step: a change of an element is calculated again, an unchanged sketch is remembered",
    "[sketch]") {
    SketchWorld world;
    const FeatureId step = world.addSketch();
    world.history.execute(std::make_unique<AddSketchElementCommand>(
        step, SketchElementKind::point, std::vector<SketchPoint>{{1, 2}}));
    const RecomputeResult first = world.calculate();
    CHECK(first.executed == 1);

    const RecomputeResult same = world.calculate();
    CHECK(same.executed == 0);
    CHECK(same.remembered == 1);
    CHECK(same.sketches.at(step) == first.sketches.at(step));

    world.history.execute(makeMoveSketchPoints(step, {{"P1", {{1, 3}}}}));
    const RecomputeResult moved = world.calculate();
    CHECK(moved.executed == 1);
    CHECK(moved.sketches.at(step).data.elements[0].points[0] == SketchPoint{1, 3});

    world.history.undo(); // back to the first state: remembered, no kernel work
    const RecomputeResult back = world.calculate();
    CHECK(back.executed == 0);
    CHECK(back.sketches.at(step) == first.sketches.at(step));
}

TEST_CASE("sketch step: next to a body, the steps do not disturb each other", "[sketch]") {
    SketchWorld world;
    world.history.addFeature(defaultDraft(*world.registry.find("box")));
    const FeatureId step = world.addSketch();
    world.history.addFeature(defaultDraft(*world.registry.find("cylinder")));
    const RecomputeResult result = world.calculate();
    CHECK(result.bodies.size() == 2);
    CHECK(result.sketches.size() == 1);
    CHECK(result.sketches.contains(step));
    CHECK(result.features[1].status == FeatureStatus::ok);

    world.history.execute(makeSetFeatureSuppressed(step, true));
    const RecomputeResult off = world.calculate();
    CHECK(off.features[1].status == FeatureStatus::suppressed);
    CHECK(off.sketches.empty());
    CHECK(off.bodies.size() == 2);
}

TEST_CASE("sketch step: a sketch on a face has no plane yet, it is never guessed", "[sketch]") {
    SketchWorld world;
    const FeatureId box = *world.history.addFeature(defaultDraft(*world.registry.find("box")));
    const BodyId body = world.history.document().bodies().front().id;
    const RecomputeResult boxResult = world.calculate();
    const BodyResult& made = boxResult.bodies.at(body);
    const BodyLooks looks = describeBody(made.faces, world.kernel->signatures(made.solid));
    REQUIRE_FALSE(looks.faces.empty());
    const auto ref = makeRef(looks, ElementKind::face, looks.faces.front().id,
                             placementOf(world.history.document(), world.registry,
                                         boxResult.features, looks.faces.front().name));
    REQUIRE(ref.has_value());
    (void)box;

    FeatureDraft draft = defaultDraft(*world.registry.find("sketch"));
    draft.inputs = {body};
    draft.refs["plane"] = {*ref};
    const FeatureId step = *world.history.addFeature(std::move(draft));
    const RecomputeResult result = world.calculate();
    CHECK(result.features[1].status == FeatureStatus::ok);
    REQUIRE(result.sketches.contains(step));
    CHECK_FALSE(result.sketches.at(step).plane.has_value());
}

TEST_CASE("sketch step: wrong words and broken data are errors on the step, not a crash",
          "[sketch]") {
    SketchWorld world;
    const FeatureId step = world.addSketch("diagonal");
    const RecomputeResult wrong = world.calculate();
    REQUIRE(wrong.features.size() == 1);
    CHECK(wrong.features[0].status == FeatureStatus::error);
    CHECK(wrong.features[0].issue->code == FeatureIssueCode::kernelRejected);
    CHECK(wrong.features[0].issue->subject == "basePlane");
    CHECK(wrong.sketches.empty());

    world.history.execute(makeSetFeatureText(step, "basePlane", "yz"));
    CHECK(world.calculate().features[0].status == FeatureStatus::ok);

    world.history.execute(makeSetFeatureValue(step, "x", "1 / 0"));
    CHECK(world.calculate().features[0].status == FeatureStatus::error);
}
