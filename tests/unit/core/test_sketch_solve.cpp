// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The mapping between a sketch step and the solver (AP-2.2b): toSolverSystem, applySolution,
// storedMeasureValues and solveSketch, with the real PlaneGCS solver.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/document.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/core/sketch.h"
#include "fission/core/sketch_solve.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/sketch/planegcs_solver.h"

using Catch::Approx;
using namespace fission::core;
using fission::geo::SketchSystem;
using fission::geo::SolveResult;
using fission::geo::SolverPoint;
using fission::geo::SolveStatus;

namespace {

SketchElement line(std::string id, double u1, double v1, double u2, double v2,
                   std::optional<SketchLink> link = std::nullopt) {
    return SketchElement{std::move(id), SketchElementKind::line, {{u1, v1}, {u2, v2}}, false, link};
}

SketchMeasure lengthMeasure(std::string id, std::vector<std::string> elements, double lastValue) {
    return SketchMeasure{std::move(id), "length", std::move(elements), {"30"}, lastValue};
}

SketchMeasure radiusMeasure(std::string id, std::vector<std::string> elements, double lastValue) {
    return SketchMeasure{std::move(id), "radius", std::move(elements), {"3"}, lastValue};
}

const SolverPoint* findPoint(const SketchSystem& system, const std::string& id) {
    for (const SolverPoint& point : system.points) {
        if (point.id == id) {
            return &point;
        }
    }
    return nullptr;
}

void checkSketchPoint(const SketchElement& element, std::size_t index, double u, double v) {
    REQUIRE(index < element.points.size());
    CHECK(element.points[index].u == Approx(u).margin(1e-9));
    CHECK(element.points[index].v == Approx(v).margin(1e-9));
}

} // namespace

// --- toSolverSystem ----------------------------------------------------------------------

TEST_CASE("toSolverSystem maps elements, rules and measures to the solver", "[sketch][solve]") {
    SketchData sketch;
    sketch.elements = {
        line("L1", 0, 0, 10, 1),
        SketchElement{"C1", SketchElementKind::circle, {{5, 5}, {8, 5}}},
        SketchElement{"P1", SketchElementKind::point, {{1, 2}}},
    };
    sketch.rules = {{"R1", "horizontal", {"L1"}}, {"R2", "coincident", {"P1", "L1.0"}}};
    sketch.measures = {lengthMeasure("R3", {"L1"}, 30.0)};

    const SketchSystem system = toSolverSystem(sketch, {{"R3", 30.0}});

    // every point exists under "<element id>.<index>"
    for (const char* id : {"L1.0", "L1.1", "C1.0", "C1.1", "P1.0"}) {
        CHECK(findPoint(system, id) != nullptr);
    }
    REQUIRE(findPoint(system, "L1.0") != nullptr);
    CHECK(findPoint(system, "L1.0")->position.x == Approx(0.0).margin(1e-9));
    CHECK(findPoint(system, "L1.1")->position.x == Approx(10.0).margin(1e-9));
    CHECK(findPoint(system, "L1.1")->position.y == Approx(1.0).margin(1e-9));

    // line and circle are curves, the point is not
    REQUIRE(system.curves.size() == 2);
    CHECK(system.curves[0].id == "L1");
    CHECK(system.curves[0].points == std::vector<std::string>({"L1.0", "L1.1"}));
    CHECK(system.curves[1].id == "C1");
    CHECK(system.curves[1].points == std::vector<std::string>({"C1.0", "C1.1"}));

    // the bare point id "P1" in the rule became "P1.0"; "L1.0" stayed
    REQUIRE(system.rules.size() == 2);
    const auto coincident =
        std::ranges::find_if(system.rules, [](const auto& rule) { return rule.id == "R2"; });
    REQUIRE(coincident != system.rules.end());
    CHECK(coincident->elements == std::vector<std::string>({"P1.0", "L1.0"}));

    REQUIRE(system.measures.size() == 1);
    CHECK(system.measures[0].id == "R3");
    CHECK(system.measures[0].elements == std::vector<std::string>({"L1"}));
    CHECK(system.measures[0].value == Approx(30.0).margin(1e-9));
}

// --- solveSketch -------------------------------------------------------------------------

TEST_CASE("solveSketch fixes a line with fixed, horizontal and length", "[sketch][solve]") {
    SketchData sketch;
    sketch.elements = {line("L1", 0, 0, 10, 1)};
    sketch.rules = {{"R1", "fixed", {"L1.0"}}, {"R2", "horizontal", {"L1"}}};
    sketch.measures = {lengthMeasure("R3", {"L1"}, 30.0)};

    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const SolveResult result = solveSketch(sketch, {{"R3", 30.0}}, *solver);

    REQUIRE(result.status == SolveStatus::ok);
    REQUIRE(result.dof == 0);
    const SketchElement& solved = sketch.elements.front();
    checkSketchPoint(solved, 0, 0, 0);
    checkSketchPoint(solved, 1, 30, 0);
}

TEST_CASE("solving the same sketch twice gives bit-identical points", "[sketch][solve]") {
    const auto build = [] {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 10, 1),
                           SketchElement{"C1", SketchElementKind::circle, {{5, 5}, {8, 5}}}};
        sketch.rules = {
            {"R1", "fixed", {"L1.0"}}, {"R2", "horizontal", {"L1"}}, {"R3", "fixed", {"C1.0"}}};
        sketch.measures = {lengthMeasure("R4", {"L1"}, 30.0), radiusMeasure("R5", {"C1"}, 3.0)};
        return sketch;
    };
    const SketchMeasureValues values = {{"R4", 30.0}, {"R5", 3.0}};

    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    SketchData firstSketch = build();
    const SolveResult first = solveSketch(firstSketch, values, *solver);
    SketchData secondSketch = build();
    const SolveResult second = solveSketch(secondSketch, values, *solver);

    REQUIRE(first.status == SolveStatus::ok);
    REQUIRE(second.status == SolveStatus::ok);
    REQUIRE(first.points.size() == second.points.size());
    for (std::size_t i = 0; i < first.points.size(); ++i) {
        CHECK(first.points[i].id == second.points[i].id);
        CHECK(first.points[i].position.x == second.points[i].position.x); // bit-identical
        CHECK(first.points[i].position.y == second.points[i].position.y);
    }
}

TEST_CASE("two lengths on one line conflict and change nothing", "[sketch][solve]") {
    SketchData sketch;
    sketch.elements = {line("L1", 0, 0, 10, 1)};
    sketch.rules = {{"R1", "fixed", {"L1.0"}}, {"R2", "horizontal", {"L1"}}};
    sketch.measures = {lengthMeasure("R3", {"L1"}, 30.0), lengthMeasure("R4", {"L1"}, 40.0)};

    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const SolveResult result = solveSketch(sketch, {{"R3", 30.0}, {"R4", 40.0}}, *solver);

    REQUIRE(result.status == SolveStatus::conflict);
    // nothing was written back
    checkSketchPoint(sketch.elements.front(), 0, 0, 0);
    checkSketchPoint(sketch.elements.front(), 1, 10, 1);

    SketchData copy = sketch;
    CHECK_FALSE(applySolution(copy, result));
    CHECK(copy == sketch);
}

TEST_CASE("a linked element keeps its points", "[sketch][solve]") {
    SketchData sketch;
    sketch.elements = {line("L1", 0, 0, 10, 0, SketchLink{0, 0}), line("L2", 0, 5, 10, 5)};
    sketch.rules = {{"R1", "fixed", {"L2.0"}}, {"R2", "horizontal", {"L2"}}};
    sketch.measures = {lengthMeasure("R3", {"L2"}, 30.0)};

    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const SolveResult result = solveSketch(sketch, {{"R3", 30.0}}, *solver);

    REQUIRE(result.status == SolveStatus::ok);
    // L2 moved to length 30, the linked L1 kept its points
    checkSketchPoint(sketch.elements[0], 0, 0, 0);
    checkSketchPoint(sketch.elements[0], 1, 10, 0);
    checkSketchPoint(sketch.elements[1], 1, 30, 5);

    // applySolution skips linked elements even when the result moves them
    SolveResult moved;
    moved.status = SolveStatus::ok;
    moved.points = {
        {"L1.0", {1.0, 1.0}}, {"L1.1", {2.0, 2.0}}, {"L2.0", {0.0, 5.0}}, {"L2.1", {30.0, 5.0}}};
    SketchData copy = sketch;
    copy.elements[0].points = {{0, 0}, {10, 0}};
    // the only moved points belong to the linked element, so nothing changes
    CHECK_FALSE(applySolution(copy, moved));
    checkSketchPoint(copy.elements[0], 0, 0, 0);
    checkSketchPoint(copy.elements[0], 1, 10, 0);
}

// --- storedMeasureValues -----------------------------------------------------------------

TEST_CASE("after solving, the free elements are known", "[core][sketch][solve][dof]") {
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();

    SECTION("a line without rules can move") {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 10, 1)};
        const SolveResult result = solveSketch(sketch, {}, *solver);
        REQUIRE(result.status == SolveStatus::ok);
        CHECK(movableElements(sketch, result) == std::vector<std::string>{"L1"});
    }

    SECTION("a fixed line cannot move") {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 10, 1)};
        sketch.rules = {{"R1", "fixed", {"L1"}}};
        const SolveResult result = solveSketch(sketch, {}, *solver);
        REQUIRE(result.status == SolveStatus::ok);
        CHECK(result.dof == 0);
        CHECK(movableElements(sketch, result).empty());
    }

    SECTION("a circle with only its centre fixed can still change its radius") {
        SketchData sketch;
        sketch.elements = {SketchElement{"C1", SketchElementKind::circle, {{5, 5}, {8, 5}}}};
        sketch.rules = {{"R1", "fixed", {"C1.0"}}};
        const SolveResult result = solveSketch(sketch, {}, *solver);
        REQUIRE(result.status == SolveStatus::ok);
        CHECK(movableElements(sketch, result) == std::vector<std::string>{"C1"});
    }

    SECTION("a circle with centre fixed and a radius measure cannot move") {
        SketchData sketch;
        sketch.elements = {SketchElement{"C1", SketchElementKind::circle, {{5, 5}, {8, 5}}}};
        sketch.rules = {{"R1", "fixed", {"C1.0"}}};
        sketch.measures = {radiusMeasure("R2", {"C1"}, 3.0)};
        const SolveResult result = solveSketch(sketch, {{"R2", 3.0}}, *solver);
        REQUIRE(result.status == SolveStatus::ok);
        CHECK(movableElements(sketch, result).empty());
    }
}

// --- storedMeasureValues -----------------------------------------------------------------

TEST_CASE("storedMeasureValues reads the last values of the measures", "[sketch][solve]") {
    SketchData sketch;
    sketch.measures = {lengthMeasure("R1", {"L1"}, 30.0), lengthMeasure("R2", {"L1"}, 40.5)};
    const SketchMeasureValues values = storedMeasureValues(sketch);
    REQUIRE(values.size() == 2);
    CHECK(values.at("R1") == Approx(30.0).margin(1e-9));
    CHECK(values.at("R2") == Approx(40.5).margin(1e-9));
}

// --- the commands that change rules and measures (AP-2.2b) -------------------------------

namespace {

FeatureDraft sketchDraft() {
    FeatureDraft draft;
    draft.type = "sketch";
    draft.sketch = SketchData{};
    return draft;
}

std::vector<SketchPoint> linePoints(const Document& document, const FeatureId& step) {
    return document.findFeature(step)->sketch->elements.front().points;
}

// Adds a line L1 (0,0)-(10,1), the rules fixed L1.0 and horizontal L1 and the length measure
// R3 = 30, so the line is solved to (30, 0). Returns the step id.
FeatureId solvedLineStep(History& history, const fission::geo::SketchSolver& solver) {
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(
        step, SketchElementKind::line, std::vector<SketchPoint>{{0, 0}, {10, 1}}));
    history.execute(makeAddSketchRule(step, "fixed", {"L1.0"}, solver));
    history.execute(makeAddSketchRule(step, "horizontal", {"L1"}, solver));
    history.execute(makeAddSketchMeasure(step, "length", {"L1"}, "30", 30.0, solver));
    return step;
}

} // namespace

TEST_CASE("sketch constraints: a measure solves the sketch, undo restores the points exactly",
          "[sketch][solve][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(
        step, SketchElementKind::line, std::vector<SketchPoint>{{0, 0}, {10, 1}}));
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    REQUIRE(history.execute(makeAddSketchRule(step, "fixed", {"L1.0"}, *solver)));
    REQUIRE(history.execute(makeAddSketchRule(step, "horizontal", {"L1"}, *solver)));
    const std::vector<SketchPoint> beforeMeasure = linePoints(history.document(), step);
    REQUIRE(history.execute(makeAddSketchMeasure(step, "length", {"L1"}, "30", 30.0, *solver)));
    const std::vector<SketchPoint> solved = linePoints(history.document(), step);
    REQUIRE(solved.size() == 2);
    CHECK(solved[1].u == Approx(30.0).margin(1e-9));
    CHECK(solved[1].v == Approx(0.0).margin(1e-9));
    REQUIRE(history.undo());
    CHECK(linePoints(history.document(), step) == beforeMeasure);
}

TEST_CASE("sketch constraints: setSketchMeasure solves again, undo and redo are exact",
          "[sketch][solve][history]") {
    History history{Document(9)};
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const FeatureId step = solvedLineStep(history, *solver);
    const std::vector<SketchPoint> at30 = linePoints(history.document(), step);
    REQUIRE(history.execute(makeSetSketchMeasure(step, "R3", "45", 45.0, *solver)));
    const std::vector<SketchPoint> at45 = linePoints(history.document(), step);
    REQUIRE(at45.size() == 2);
    CHECK(at45[1].u == Approx(45.0).margin(1e-9));
    CHECK(at45[1].v == Approx(0.0).margin(1e-9));
    REQUIRE(history.undo());
    CHECK(linePoints(history.document(), step) == at30);
    REQUIRE(history.redo());
    CHECK(linePoints(history.document(), step) == at45);
}

TEST_CASE("sketch constraints: removeSketchConstraint takes the measure out, undo brings it back",
          "[sketch][solve][history]") {
    History history{Document(9)};
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const FeatureId step = solvedLineStep(history, *solver);
    const std::vector<SketchPoint> at30 = linePoints(history.document(), step);
    REQUIRE(history.execute(makeRemoveSketchConstraint(step, "R3")));
    const SketchData& removed = *history.document().findFeature(step)->sketch;
    CHECK(removed.measures.empty());
    CHECK(removed.elements.front().points == at30); // no solving: the points stay
    REQUIRE(history.undo());
    CHECK(history.document().findFeature(step)->sketch->measures.size() == 1);
}

TEST_CASE("sketch constraints: a conflicting measure is stored but changes no points",
          "[sketch][solve][history]") {
    History history{Document(9)};
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const FeatureId step = solvedLineStep(history, *solver);
    const std::vector<SketchPoint> before = linePoints(history.document(), step);
    REQUIRE(history.execute(makeAddSketchMeasure(step, "length", {"L1"}, "40", 40.0, *solver)));
    const SketchData& after = *history.document().findFeature(step)->sketch;
    CHECK(after.measures.size() == 2);
    CHECK(after.elements.front().points == before);
}

TEST_CASE("sketch constraints: the commands describe themselves with keys and values",
          "[sketch][solve][history]") {
    History history{Document(9)};
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    solvedLineStep(history, *solver);
    const auto& steps = history.steps();
    REQUIRE(steps.size() == 5);
    CHECK(steps[2].key == "addSketchRule");
    CHECK(steps[2].values.at("rule") == "fixed");
    CHECK(steps[3].key == "addSketchRule");
    CHECK(steps[3].values.at("rule") == "horizontal");
    CHECK(steps[4].key == "addSketchMeasure");
    CHECK(steps[4].values.at("measure") == "length");
    CHECK(steps[4].values.at("value") == "30");
}

// --- the recompute evaluates the measures and solves the sketch (AP-2.2b) ------------------

namespace {

struct MeasureWorld {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    Document document{31};

    MeasureWorld() { fission::features::registerBuiltinFeatures(registry); }

    // A sketch step with line L1 (0,0)-(10,1), the rules fixed L1.0 and horizontal L1, and the
    // length measure R3 on L1 with the given expression.
    FeatureId addSolvedLine(const std::string& expression, double lastValue) {
        FeatureDraft draft = defaultDraft(*registry.find("sketch"));
        SketchData& data = *draft.sketch;
        data.elements = {line(data.allocateId(SketchElementKind::line), 0, 0, 10, 1)};
        data.rules = {{data.allocateRuleId(), "fixed", {"L1.0"}},
                      {data.allocateRuleId(), "horizontal", {"L1"}}};
        data.measures = {
            SketchMeasure{data.allocateRuleId(), "length", {"L1"}, {expression}, lastValue}};
        return *document.addFeature(std::move(draft));
    }

    RecomputeResult run(const fission::geo::SketchSolver& solver) {
        const auto result = recompute(document, registry, *kernel, cache, {}, &solver);
        REQUIRE(result.has_value());
        return *result;
    }
};

} // namespace

TEST_CASE("recompute: a sketch measure that uses a named measure follows the named measure",
          "[sketch][solve][recompute]") {
    MeasureWorld world;
    world.document.addParameter({"width", "30", QuantityKind::length, ""});
    const FeatureId step = world.addSolvedLine("width", 30.0);
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();

    const RecomputeResult first = world.run(*solver);
    const FeatureState* state = first.state(step);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    REQUIRE(first.sketches.contains(step));
    const SketchResult& sketch = first.sketches.at(step);
    REQUIRE(sketch.data.elements.size() == 1);
    CHECK(sketch.data.elements[0].points[1].u == Approx(30.0).margin(1e-9));
    CHECK(sketch.data.elements[0].points[1].v == Approx(0.0).margin(1e-9));
    REQUIRE(sketch.data.measures.size() == 1);
    CHECK(sketch.data.measures[0].lastValue == Approx(30.0).margin(1e-9));

    // The named measure changed: the measure is evaluated again and the step solved again.
    world.document.setParameterExpression("width", "45");
    const RecomputeResult second = world.run(*solver);
    CHECK(second.state(step)->status == FeatureStatus::ok);
    CHECK(second.executed == 1); // not remembered: the measure values are in the cache key
    const SketchResult& moved = second.sketches.at(step);
    CHECK(moved.data.elements[0].points[1].u == Approx(45.0).margin(1e-9));
    CHECK(moved.data.elements[0].points[1].v == Approx(0.0).margin(1e-9));
    CHECK(moved.data.measures[0].lastValue == Approx(45.0).margin(1e-9));
}

TEST_CASE("recompute: a sketch measure with a broken formula is an error on the step",
          "[sketch][solve][recompute]") {
    MeasureWorld world;
    const FeatureId step = world.addSolvedLine("nonsense +", 30.0);
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const RecomputeResult result = world.run(*solver);
    const FeatureState* state = result.state(step);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::parameterInvalid);
    CHECK(state->issue->subject == "R3"); // the measure id
    REQUIRE(state->issue->expression.has_value());
    CHECK_FALSE(result.sketches.contains(step));
}

// --- the rules the drawing fulfils are added when the elements are drawn (AP-2.2c2) --------

TEST_CASE("drawing adds the rules it fulfils", "[core][sketch][autorules]") {
    const auto hasRule = [](const std::vector<AutoRule>& rules, const AutoRule& rule) {
        return std::ranges::find(rules, rule) != rules.end();
    };

    SECTION("a level line is horizontal") {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 10, 0)};
        const std::vector<AutoRule> rules = autoSketchRules(sketch, {"L1"});
        CHECK(rules == std::vector<AutoRule>{{"horizontal", {"L1"}}});
    }

    SECTION("a rectangle drawn in one go is closed") {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 40, 0), line("L2", 40, 0, 40, 30),
                           line("L3", 40, 30, 0, 30), line("L4", 0, 30, 0, 0)};
        const std::vector<AutoRule> rules = autoSketchRules(sketch, {"L1", "L2", "L3", "L4"});
        const auto count = [&](const std::string& kind) {
            return std::ranges::count_if(rules, [&](const AutoRule& r) { return r.kind == kind; });
        };
        CHECK(count("horizontal") == 2);
        CHECK(count("vertical") == 2);
        CHECK(count("coincident") == 4);
        CHECK(rules.size() == 8);
    }

    SECTION("a slanted line fulfils no rule") {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 10, 3)};
        CHECK(autoSketchRules(sketch, {"L1"}).empty());
    }

    SECTION("a rule the sketch already has is left out") {
        SketchData sketch;
        sketch.elements = {line("L1", 0, 0, 10, 0)};
        sketch.rules = {{"R1", "horizontal", {"L1"}}};
        CHECK(autoSketchRules(sketch, {"L1"}).empty());
    }

    SECTION("an arc starting at the end of a line is coincident and tangent") {
        SketchData sketch;
        sketch.elements = {
            line("L1", 0, 0, 10, 0),
            SketchElement{"A1", SketchElementKind::arc, {{10, 5}, {10, 0}, {15, 5}}}};
        const std::vector<AutoRule> rules = autoSketchRules(sketch, {"A1"});
        CHECK(hasRule(rules, {"coincident", {"A1.1", "L1.1"}}));
        CHECK(hasRule(rules, {"tangent", {"A1", "L1"}}));
    }
}
