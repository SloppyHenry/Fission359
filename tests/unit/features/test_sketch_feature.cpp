// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The sketch step (AP-2.1a, ADR-0013): makes no body, its result is the plane and the elements,
// changes are seen by the recompute, and the step is registered like every other type.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>
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

// --- a sketch on a face (AP-2.1b) ------------------------------------------------------------

namespace {

struct FaceSketch {
    SketchWorld world;
    FeatureId shape;
    BodyId body;

    // A box (or another shape) with the given values.
    void make(const std::string& type, const std::map<std::string, std::string>& values) {
        FeatureDraft draft = defaultDraft(*world.registry.find(type));
        for (const auto& [name, expression] : values) {
            draft.params.at(name).expression = expression;
        }
        draft.newBodies = 1;
        shape = *world.history.addFeature(std::move(draft));
        body = world.history.document().bodies().front().id;
    }

    // The reference to the face with this name, e.g. "F1/box/top".
    PersistentRef faceRef(const std::string& name) {
        const RecomputeResult result = world.calculate();
        const BodyResult& made = result.bodies.at(body);
        const BodyLooks looks = describeBody(made.faces, world.kernel->signatures(made.solid));
        for (const NamedLook& face : looks.faces) {
            if (face.name == name) {
                const auto ref = makeRef(looks, ElementKind::face, face.id,
                                         placementOf(world.history.document(), world.registry,
                                                     result.features, face.name));
                REQUIRE(ref.has_value());
                return *ref;
            }
        }
        FAIL("no face named " << name);
        return {};
    }

    FeatureId sketchOn(const std::string& name) {
        FeatureDraft draft = defaultDraft(*world.registry.find("sketch"));
        draft.inputs = {body};
        draft.refs["plane"] = {faceRef(name)};
        return *world.history.addFeature(std::move(draft));
    }

    // The plane of the sketch; the step must have calculated.
    SketchPlane planeOf(const FeatureId& step) {
        const RecomputeResult result = world.calculate();
        REQUIRE(result.state(step)->status == FeatureStatus::ok);
        REQUIRE(result.sketches.contains(step));
        REQUIRE(result.sketches.at(step).plane.has_value());
        return *result.sketches.at(step).plane;
    }
};

void checkSquare(const SketchPlane& plane) {
    CHECK(plane.uAxis().x * plane.vAxis().x + plane.uAxis().y * plane.vAxis().y +
              plane.uAxis().z * plane.vAxis().z ==
          Approx(0.0).margin(1e-12));
    CHECK(plane.normal().x * plane.normal().x + plane.normal().y * plane.normal().y +
              plane.normal().z * plane.normal().z ==
          Approx(1.0));
}

} // namespace

TEST_CASE("sketch step: on the top of a box the plane lies in the top, in its middle",
          "[sketch][plane]") {
    FaceSketch test;
    test.make("box", {{"width", "40"}, {"depth", "20"}, {"height", "10"}});
    const SketchPlane plane = test.planeOf(test.sketchOn("F1/box/top"));
    checkSquare(plane);
    CHECK(plane.origin().x == Approx(0.0).margin(1e-9));
    CHECK(plane.origin().y == Approx(0.0).margin(1e-9));
    CHECK(plane.origin().z == Approx(10.0));
    CHECK(plane.normal().z == Approx(1.0)); // looks up, like the face
    // u runs along the long edge (X), v across the face.
    CHECK(std::abs(plane.uAxis().x) == Approx(1.0));
    CHECK(std::abs(plane.vAxis().y) == Approx(1.0));
}

TEST_CASE("sketch step: on a side face the plane stands up and looks out of the box",
          "[sketch][plane]") {
    FaceSketch test;
    test.make("box", {{"width", "40"}, {"depth", "20"}, {"height", "10"}});
    const SketchPlane right = test.planeOf(test.sketchOn("F1/box/right"));
    checkSquare(right);
    CHECK(right.normal().x == Approx(1.0));
    CHECK(right.origin().x == Approx(20.0));
    CHECK(right.origin().z == Approx(5.0));
    // The right face is 20 wide (Y) and 10 high (Z): u runs along Y, its longest edge.
    CHECK(std::abs(right.uAxis().y) == Approx(1.0));

    const SketchPlane front = test.planeOf(test.sketchOn("F1/box/front"));
    CHECK(front.normal().y == Approx(-1.0));
    CHECK(std::abs(front.uAxis().x) == Approx(1.0)); // 40 mm wide: the longest edge
    // A point of the plane is on the face: (u, v) = (0, 0) is its middle.
    const auto middle = front.toWorld({0.0, 0.0});
    CHECK(middle.y == Approx(-10.0));
    CHECK(middle.z == Approx(5.0));
}

TEST_CASE("sketch step: the plane follows a body that is moved and turned", "[sketch][plane]") {
    FaceSketch test;
    test.make("box", {{"width", "40"}, {"depth", "20"}, {"height", "10"}});
    const FeatureId step = test.sketchOn("F1/box/top");
    const SketchPlane before = test.planeOf(step);

    // Moved: the same plane, 100 mm to the right and 30 mm back.
    REQUIRE(test.world.history.document().findFeature(test.shape) != nullptr);
    test.world.history.execute(makeSetFeatureValue(test.shape, "x", "100"));
    test.world.history.execute(makeSetFeatureValue(test.shape, "y", "30"));
    const SketchPlane moved = test.planeOf(step);
    CHECK(moved.origin().x == Approx(100.0).margin(1e-9));
    CHECK(moved.origin().y == Approx(30.0).margin(1e-9));
    CHECK(moved.origin().z == Approx(10.0));
    CHECK(moved.uAxis().x == Approx(before.uAxis().x));
    CHECK(moved.vAxis().y == Approx(before.vAxis().y));

    // Turned a quarter about Z: the plane turns with the body, its u axis now runs along Y.
    test.world.history.execute(makeSetFeatureValue(test.shape, "x", "0"));
    test.world.history.execute(makeSetFeatureValue(test.shape, "y", "0"));
    test.world.history.execute(makeSetFeatureValue(test.shape, "rotZ", "90"));
    const SketchPlane turned = test.planeOf(step);
    checkSquare(turned);
    CHECK(std::abs(turned.uAxis().y) == Approx(1.0));
    CHECK(turned.normal().z == Approx(1.0));
    // The turn is exactly the body's: u = before.u turned by 90 degrees about Z.
    CHECK(turned.uAxis().x == Approx(-before.uAxis().y).margin(1e-9));
    CHECK(turned.uAxis().y == Approx(before.uAxis().x).margin(1e-9));
}

TEST_CASE("sketch step: on a tilted face the plane is the face's own", "[sketch][plane]") {
    FaceSketch test;
    test.make("box", {{"width", "40"}, {"depth", "20"}, {"height", "10"}, {"rotX", "30"}});
    const SketchPlane plane = test.planeOf(test.sketchOn("F1/box/top"));
    checkSquare(plane);
    CHECK(plane.normal().x == Approx(0.0).margin(1e-9));
    CHECK(plane.normal().y == Approx(-std::sin(30.0 * 3.14159265358979323846 / 180.0)));
    CHECK(plane.normal().z == Approx(std::cos(30.0 * 3.14159265358979323846 / 180.0)));
    CHECK(std::abs(plane.uAxis().x) == Approx(1.0)); // the long edge stays along X
}

TEST_CASE("sketch step: a curved face is refused, as an error on the step", "[sketch][plane]") {
    FaceSketch test;
    test.make("cylinder", {});
    const FeatureId step = test.sketchOn("F1/cylinder/side");
    const RecomputeResult result = test.world.calculate();
    CHECK(result.state(step)->status == FeatureStatus::error);
    CHECK(result.state(step)->issue->code == FeatureIssueCode::kernelRejected);
    CHECK(result.state(step)->issue->subject == "plane"); // the message catalogue speaks of it
    CHECK_FALSE(result.sketches.contains(step));
    // The flat top of the same cylinder is fine.
    const FeatureId top = test.sketchOn("F1/cylinder/top");
    CHECK(test.planeOf(top).normal().z == Approx(1.0));
}

TEST_CASE("sketch step: a face that cannot be found again needs help, nothing is guessed",
          "[sketch][plane]") {
    FaceSketch test;
    test.make("box", {});
    FeatureDraft draft = defaultDraft(*test.world.registry.find("sketch"));
    draft.refs["plane"] = {test.faceRef("F1/box/top")}; // no body to look in
    const FeatureId lonely = *test.world.history.addFeature(std::move(draft));
    const RecomputeResult result = test.world.calculate();
    const FeatureState* state = result.state(lonely);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::warning);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::referenceNeedsHelp);
    CHECK_FALSE(result.sketches.contains(lonely));
}

TEST_CASE("sketch step: a box with a sketch on it stays a whole body", "[sketch][plane]") {
    FaceSketch test;
    test.make("box", {});
    (void)test.sketchOn("F1/box/top");
    // The sketch only reads the face: the body is not used up by it.
    CHECK_FALSE(test.world.history.document().consumerOf(test.body).has_value());
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
