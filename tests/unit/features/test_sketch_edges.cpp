// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Edges taken over into a sketch (AP-2.1f, decisions 45-49): the element follows its edge when the
// body changes, an edge that is not found safely needs help and nothing moves, an element that was
// let go is a plain one, and Undo/Redo bring the ids back exactly.

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

struct EdgeWorld {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    History history{Document(31)};
    FeatureId shape; ///< the first box
    BodyId body;     ///< its body

    EdgeWorld() {
        fission::features::registerBuiltinFeatures(registry);
        shape = addBox({{"width", "20"}, {"depth", "20"}, {"height", "10"}});
        body = history.document().bodies().front().id;
    }

    FeatureId addBox(const std::map<std::string, std::string>& values) {
        FeatureDraft draft = defaultDraft(*registry.find("box"));
        for (const auto& [name, expression] : values) {
            draft.params.at(name).expression = expression;
        }
        draft.newBodies = 1;
        return *history.addFeature(std::move(draft));
    }

    RecomputeResult calculate() {
        const auto result = recompute(history.document(), registry, *kernel, cache);
        REQUIRE(result.has_value());
        return *result;
    }

    // A reference to the face or edge of `bodyId` called `name`.
    PersistentRef refTo(const BodyId& bodyId, ElementKind kind, const std::string& name) {
        const RecomputeResult result = calculate();
        const BodyResult& made = result.bodies.at(bodyId);
        const BodyLooks looks = describeBody(made.faces, kernel->signatures(made.solid));
        const auto& list = kind == ElementKind::face ? looks.faces : looks.edges;
        for (const NamedLook& element : list) {
            if (element.name == name) {
                const auto ref = makeRef(
                    looks, kind, element.id,
                    placementOf(history.document(), registry, result.features, element.name));
                REQUIRE(ref.has_value());
                return *ref;
            }
        }
        FAIL("no element named " << name);
        return {};
    }

    // A sketch on the top of the first box.
    FeatureId sketchOnTop() {
        FeatureDraft draft = defaultDraft(*registry.find("sketch"));
        draft.inputs = {body};
        draft.refs["plane"] = {refTo(body, ElementKind::face, "F1/box/top")};
        return *history.addFeature(std::move(draft));
    }

    // Takes the edge over as one step of the history: input, reference and element.
    std::string takeOver(const FeatureId& step, const BodyId& from, const std::string& name,
                         SketchElementKind kind, std::vector<SketchPoint> points) {
        const PersistentRef ref = refTo(from, ElementKind::edge, name);
        const Feature* feature = history.document().findFeature(step);
        std::vector<BodyId> inputs = feature->inputs;
        auto at = std::find(inputs.begin(), inputs.end(), from);
        if (at == inputs.end()) {
            inputs.push_back(from);
            at = inputs.end() - 1;
        }
        std::vector<PersistentRef> refs = feature->refs.contains("edges")
                                              ? feature->refs.at("edges")
                                              : std::vector<PersistentRef>{};
        refs.push_back(ref);
        history.beginTransaction({"takeOverEdge", {}});
        history.execute(makeSetFeatureInputs(step, inputs));
        history.execute(makeSetFeatureRefs(step, "edges", refs));
        auto add = std::make_unique<AddSketchElementCommand>(
            step, kind, std::move(points), false,
            SketchLink{static_cast<std::size_t>(at - inputs.begin()), refs.size() - 1});
        const AddSketchElementCommand* added = add.get();
        REQUIRE(history.execute(std::move(add)));
        const std::string id = *added->result();
        history.commitTransaction();
        return id;
    }

    const SketchElement& element(const FeatureId& step, const std::string& id) {
        return *history.document().findFeature(step)->sketch->find(id);
    }
};

bool near(const SketchPoint& a, const SketchPoint& b) {
    return std::abs(a.u - b.u) < 1e-9 && std::abs(a.v - b.v) < 1e-9;
}

// The line's end points, in either order, as plane coordinates of the world points.
bool lineIs(const SketchElement& line, const SketchPlane& plane, const fission::geo::Point3d& a,
            const fission::geo::Point3d& b) {
    const SketchPoint pa = plane.toPlane(a);
    const SketchPoint pb = plane.toPlane(b);
    return line.points.size() == 2 && ((near(line.points[0], pa) && near(line.points[1], pb)) ||
                                       (near(line.points[0], pb) && near(line.points[1], pa)));
}

const char* kRight = "edge[F1/box/right|F1/box/top]";

} // namespace

TEST_CASE("edges taken over: the line follows the body when it grows from 20 to 30 mm",
          "[sketch][edge][refs]") {
    EdgeWorld world;
    const FeatureId step = world.sketchOnTop();
    const SketchPlane plane = *world.calculate().sketches.at(step).plane;
    const std::string line =
        world.takeOver(step, world.body, kRight, SketchElementKind::line,
                       {plane.toPlane({10, -10, 10}), plane.toPlane({10, 10, 10})});
    REQUIRE(line == "L1");

    RecomputeResult result = world.calculate();
    REQUIRE(result.state(step)->status == FeatureStatus::ok);
    CHECK(lineIs(result.sketches.at(step).data.elements[0], plane, {10, -10, 10}, {10, 10, 10}));

    world.history.execute(makeSetFeatureValue(world.shape, "width", "30"));
    result = world.calculate();
    REQUIRE(result.state(step)->status == FeatureStatus::ok);
    const SketchPlane grown = *result.sketches.at(step).plane;
    // the plane is the top face's: its middle stays where the face's middle is, the edge moves out
    CHECK(lineIs(result.sketches.at(step).data.elements[0], grown, {15, -10, 10}, {15, 10, 10}));
    // the document keeps the last points (written by the upkeep that follows every calculation)
    CHECK(world.history.mendRefs(result) >= 1); // the element, and the looks of the references
    CHECK(lineIs(world.element(step, "L1"), grown, {15, -10, 10}, {15, 10, 10}));
    // and the same calculation again finds nothing more to write
    result = world.calculate();
    CHECK(world.history.mendRefs(result) == 0);
    CHECK(lineIs(world.element(step, "L1"), grown, {15, -10, 10}, {15, 10, 10}));
}

TEST_CASE("edges taken over: a circle follows its body, an arc keeps its sense",
          "[sketch][edge][refs]") {
    EdgeWorld world;
    // A cylinder: take the circle of its top over, then make the cylinder wider.
    FeatureDraft draft = defaultDraft(*world.registry.find("cylinder"));
    draft.params.at("radius").expression = "7";
    draft.params.at("height").expression = "10";
    draft.newBodies = 1;
    const FeatureId cylinder = *world.history.addFeature(std::move(draft));
    const BodyId body = world.history.document().bodies().back().id;
    FeatureDraft sketch = defaultDraft(*world.registry.find("sketch"));
    sketch.inputs = {body};
    sketch.refs["plane"] = {world.refTo(body, ElementKind::face, "F2/cylinder/top")};
    const FeatureId step = *world.history.addFeature(std::move(sketch));
    REQUIRE(world.calculate().sketches.contains(step));
    const std::string circle = world.takeOver(step, body, "edge[F2/cylinder/side|F2/cylinder/top]",
                                              SketchElementKind::circle, {{0, 0}, {7, 0}});
    CHECK(circle == "C1");
    world.history.execute(makeSetFeatureValue(cylinder, "radius", "9"));
    const RecomputeResult result = world.calculate();
    REQUIRE(result.state(step)->status == FeatureStatus::ok);
    const SketchElement& element = result.sketches.at(step).data.elements[0];
    CHECK(element.kind == SketchElementKind::circle);
    CHECK(std::hypot(element.points[1].u - element.points[0].u,
                     element.points[1].v - element.points[0].v) == Approx(9.0).margin(1e-9));
    CHECK(std::hypot(element.points[0].u, element.points[0].v) == Approx(0.0).margin(1e-9));
}

TEST_CASE("edges taken over: an edge that cannot be found needs help and nothing moves",
          "[sketch][edge][refs]") {
    EdgeWorld world;
    const FeatureId step = world.sketchOnTop();
    const SketchPlane plane = *world.calculate().sketches.at(step).plane;
    world.takeOver(step, world.body, kRight, SketchElementKind::line,
                   {plane.toPlane({10, -10, 10}), plane.toPlane({10, 10, 10})});
    RecomputeResult result = world.calculate();
    REQUIRE(result.state(step)->status == FeatureStatus::ok);
    const std::vector<SketchPoint> before = world.element(step, "L1").points;

    // The reference no longer matches anything: another name, a place and a size nobody has.
    std::vector<PersistentRef> refs = world.history.document().findFeature(step)->refs.at("edges");
    refs[0].name = "edge[F9/box/left|F9/box/top]";
    refs[0].look.centroid = {500.0, 500.0, 500.0};
    refs[0].look.size = 1234.0;
    refs[0].look.neighbours = {"F9/box/left", "F9/box/top"};
    refs[0].look.neighbourLooks.clear();
    world.history.execute(makeSetFeatureRefs(step, "edges", refs));
    result = world.calculate();
    const FeatureState* state = result.state(step);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::warning);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::referenceNeedsHelp);
    CHECK(state->issue->subject == "edges");
    CHECK(state->issue->refIndex == 0);
    CHECK(state->issue->refKind == ElementKind::edge);
    // Nothing is moved: the element has the points it last had, and nothing is written.
    CHECK(world.history.mendRefs(result) == 0);
    CHECK(world.element(step, "L1").points == before);
}

TEST_CASE("edges taken over: an edge from another body, the plane from the first",
          "[sketch][edge][refs]") {
    EdgeWorld world;
    const FeatureId other =
        world.addBox({{"width", "10"}, {"depth", "10"}, {"height", "10"}, {"x", "50"}});
    const BodyId second = world.history.document().bodies().back().id;
    const FeatureId step = world.sketchOnTop();
    const SketchPlane plane = *world.calculate().sketches.at(step).plane;
    // F2 is the second box: its top-right edge runs at x = 55, z = 10
    world.takeOver(step, second, "edge[F2/box/right|F2/box/top]", SketchElementKind::line,
                   {plane.toPlane({55, -5, 10}), plane.toPlane({55, 5, 10})});
    const Feature* feature = world.history.document().findFeature(step);
    REQUIRE(feature->inputs.size() == 2);
    CHECK(feature->sketch->elements[0].link == SketchLink{1, 0});
    RecomputeResult result = world.calculate();
    REQUIRE(result.state(step)->status == FeatureStatus::ok);
    CHECK(lineIs(result.sketches.at(step).data.elements[0], plane, {55, -5, 10}, {55, 5, 10}));
    // The second box moves: the line goes along
    world.history.execute(makeSetFeatureValue(other, "x", "70"));
    result = world.calculate();
    REQUIRE(result.state(step)->status == FeatureStatus::ok);
    CHECK(lineIs(result.sketches.at(step).data.elements[0], *result.sketches.at(step).plane,
                 {75, -5, 10}, {75, 5, 10}));
}

TEST_CASE("edges taken over: letting go makes a plain element, the reference stays unused",
          "[sketch][edge][refs]") {
    EdgeWorld world;
    const FeatureId step = world.sketchOnTop();
    const SketchPlane plane = *world.calculate().sketches.at(step).plane;
    world.takeOver(step, world.body, kRight, SketchElementKind::line,
                   {plane.toPlane({10, -10, 10}), plane.toPlane({10, 10, 10})});
    // A move of the points of a linked element is refused
    CHECK_FALSE(world.history.execute(makeMoveSketchPoints(step, {{"L1", {{0, 0}, {1, 1}}}})));

    REQUIRE(world.history.execute(makeSetSketchLink(step, "L1", std::nullopt)));
    CHECK_FALSE(world.element(step, "L1").link.has_value());
    // The element is free now: it can be moved, and the unused reference is not looked up
    CHECK(world.history.execute(makeMoveSketchPoints(step, {{"L1", {{0, 0}, {1, 1}}}})));
    std::vector<PersistentRef> refs = world.history.document().findFeature(step)->refs.at("edges");
    refs[0].name = "edge[F9/box/left|F9/box/top]";
    refs[0].look.centroid = {500.0, 500.0, 500.0};
    refs[0].look.size = 1234.0;
    world.history.execute(makeSetFeatureRefs(step, "edges", refs));
    const RecomputeResult result = world.calculate();
    CHECK(result.state(step)->status == FeatureStatus::ok);
    CHECK(world.element(step, "L1").points == std::vector<SketchPoint>{{0, 0}, {1, 1}});
}

TEST_CASE("edges taken over: Undo and Redo bring back the ids, the link and the reference",
          "[sketch][edge][refs]") {
    EdgeWorld world;
    const FeatureId step = world.sketchOnTop();
    const SketchPlane plane = *world.calculate().sketches.at(step).plane;
    const Feature* before = world.history.document().findFeature(step);
    const std::vector<BodyId> inputsBefore = before->inputs;

    const std::string first =
        world.takeOver(step, world.body, kRight, SketchElementKind::line,
                       {plane.toPlane({10, -10, 10}), plane.toPlane({10, 10, 10})});
    REQUIRE(first == "L1");
    const Document taken = world.history.document();
    REQUIRE(world.history.undo()); // the whole take-over is one step
    const Feature* undone = world.history.document().findFeature(step);
    CHECK(undone->sketch->elements.empty());
    CHECK((!undone->refs.contains("edges") || undone->refs.at("edges").empty()));
    CHECK(undone->inputs == inputsBefore);
    REQUIRE(world.history.redo());
    CHECK(world.history.document().sameContentAs(taken));
    CHECK(world.element(step, "L1").link == SketchLink{0, 0});

    // Letting go and Undo
    REQUIRE(world.history.execute(makeSetSketchLink(step, "L1", std::nullopt)));
    REQUIRE(world.history.undo());
    CHECK(world.element(step, "L1").link == SketchLink{0, 0});
    REQUIRE(world.history.redo());
    CHECK_FALSE(world.element(step, "L1").link.has_value());
}
