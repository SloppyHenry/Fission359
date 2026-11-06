// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Sketches in the document (AP-2.1a, ADR-0013): the plane maths, ids, the data in the step, and
// the commands that change it with their Undo.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/document.h"
#include "fission/core/history.h"
#include "fission/core/sketch.h"

using Catch::Approx;
using namespace fission::core;
using fission::geo::Angles;
using fission::geo::Point3d;

namespace {

FeatureDraft sketchDraft() {
    FeatureDraft draft;
    draft.type = "sketch";
    draft.sketch = SketchData{};
    return draft;
}

std::vector<SketchPoint> segment(double u1, double v1, double u2, double v2) {
    return {{u1, v1}, {u2, v2}};
}

void checkPoint(const Point3d& got, double x, double y, double z) {
    CHECK(got.x == Approx(x).margin(1e-9));
    CHECK(got.y == Approx(y).margin(1e-9));
    CHECK(got.z == Approx(z).margin(1e-9));
}

} // namespace

// --- the plane ---------------------------------------------------------------------------

TEST_CASE("sketch plane: the three base planes have the documented axes", "[sketch]") {
    const SketchPlane xy = placedBasePlane(BasePlane::xy, {}, {});
    checkPoint(xy.toWorld({3, 4}), 3, 4, 0);
    checkPoint(xy.normal(), 0, 0, 1);

    const SketchPlane xz = placedBasePlane(BasePlane::xz, {}, {});
    checkPoint(xz.toWorld({3, 4}), 3, 0, 4);
    checkPoint(xz.normal(), 0, -1, 0); // towards the viewer in the front view

    const SketchPlane yz = placedBasePlane(BasePlane::yz, {}, {});
    checkPoint(yz.toWorld({3, 4}), 0, 3, 4);
    checkPoint(yz.normal(), 1, 0, 0);
}

TEST_CASE("sketch plane: toPlane undoes toWorld, also for a moved and turned plane", "[sketch]") {
    const SketchPlane plane = placedBasePlane(BasePlane::xz, {10, -5, 7}, Angles{20, 35, -50});
    for (const SketchPoint point : {SketchPoint{0, 0}, {12.5, -3.25}, {-40, 99}}) {
        const SketchPoint back = plane.toPlane(plane.toWorld(point));
        CHECK(back.u == Approx(point.u).margin(1e-9));
        CHECK(back.v == Approx(point.v).margin(1e-9));
        CHECK(plane.heightOf(plane.toWorld(point)) == Approx(0.0).margin(1e-9));
    }
    // The axes stay perpendicular unit vectors, the normal is u x v.
    const auto dot = [](const Point3d& a, const Point3d& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    CHECK(dot(plane.uAxis(), plane.uAxis()) == Approx(1.0));
    CHECK(dot(plane.vAxis(), plane.vAxis()) == Approx(1.0));
    CHECK(dot(plane.uAxis(), plane.vAxis()) == Approx(0.0).margin(1e-12));
    CHECK(dot(plane.normal(), plane.uAxis()) == Approx(0.0).margin(1e-12));
    CHECK(dot(plane.normal(), plane.vAxis()) == Approx(0.0).margin(1e-12));
}

TEST_CASE("sketch plane: a turn about Z turns the axes, the origin stays", "[sketch]") {
    const SketchPlane plane = placedBasePlane(BasePlane::xy, {5, 6, 7}, Angles{0, 0, 90});
    checkPoint(plane.origin(), 5, 6, 7);
    checkPoint(plane.uAxis(), 0, 1, 0);
    checkPoint(plane.vAxis(), -1, 0, 0);
    checkPoint(plane.toWorld({2, 3}), 2, 8, 7);
}

TEST_CASE("sketch plane: a point off the plane is projected, its height is kept apart",
          "[sketch]") {
    const SketchPlane plane = placedBasePlane(BasePlane::xy, {0, 0, 5}, {});
    const SketchPoint on = plane.toPlane({3, 4, 9});
    CHECK(on.u == Approx(3.0));
    CHECK(on.v == Approx(4.0));
    CHECK(plane.heightOf({3, 4, 9}) == Approx(4.0));
}

TEST_CASE("sketch plane: axes that make no plane give the ground plane", "[sketch]") {
    CHECK(SketchPlane({1, 2, 3}, {0, 0, 0}, {0, 1, 0}).uAxis() == Point3d{1, 0, 0});
    const SketchPlane parallel({1, 2, 3}, {1, 0, 0}, {2, 0, 0});
    CHECK(parallel.normal() == Point3d{0, 0, 1});
    CHECK(parallel.origin() == Point3d{1, 2, 3});
    // A second axis that is not perpendicular is made so.
    const SketchPlane slanted({}, {1, 0, 0}, {1, 1, 0});
    checkPoint(slanted.vAxis(), 0, 1, 0);
}

TEST_CASE("sketch plane: base plane words", "[sketch]") {
    for (const BasePlane plane : {BasePlane::xy, BasePlane::xz, BasePlane::yz}) {
        CHECK(basePlaneFromWord(basePlaneWord(plane)) == plane);
    }
    CHECK_FALSE(basePlaneFromWord("ab"));
    CHECK_FALSE(basePlaneFromWord(""));
}

// --- ids and elements -------------------------------------------------------------------------

TEST_CASE("sketch ids count up per kind and are read back", "[sketch]") {
    SketchData data;
    CHECK(data.allocateId(SketchElementKind::line) == "L1");
    CHECK(data.allocateId(SketchElementKind::line) == "L2");
    CHECK(data.allocateId(SketchElementKind::point) == "P1");
    CHECK(data.allocateId(SketchElementKind::circle) == "C1");
    CHECK(data.allocateId(SketchElementKind::arc) == "A1");
    CHECK(data.allocateId(SketchElementKind::spline) == "S1");

    CHECK(parseSketchId("L12")->kind == SketchElementKind::line);
    CHECK(parseSketchId("L12")->number == 12);
    for (const char* bad :
         {"", "L", "L0", "L01", "X1", "l1", "L1a", "L-1", "LL1", "L99999999999999"}) {
        INFO(bad);
        CHECK_FALSE(parseSketchId(bad));
    }
}

TEST_CASE("sketch elements: the right number of finite points per kind", "[sketch]") {
    const SketchPoint a{0, 0};
    const SketchPoint b{1, 1};
    const SketchPoint c{2, 0};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(sketchPointsUsable(SketchElementKind::point, {a}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::point, {a, b}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::point, {}));
    CHECK(sketchPointsUsable(SketchElementKind::line, {a, b}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::line, {a, b, c}));
    CHECK(sketchPointsUsable(SketchElementKind::circle, {a, b}));
    CHECK(sketchPointsUsable(SketchElementKind::arc, {a, b, c}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::arc, {a, b}));
    CHECK(sketchPointsUsable(SketchElementKind::spline, {a, b}));
    CHECK(sketchPointsUsable(SketchElementKind::spline, {a, b, c, a, b}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::spline, {a}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::line, {a, {nan, 0}}));
    CHECK_FALSE(sketchPointsUsable(SketchElementKind::line, {a, {0, inf}}));
}

TEST_CASE("sketch data: consistent needs unique ids within the counters", "[sketch]") {
    SketchData data;
    data.elements.push_back({"L1", SketchElementKind::line, segment(0, 0, 1, 0), false});
    CHECK_FALSE(data.consistent()); // the counter is still 0: L1 could be given out again
    data.lastNumber[static_cast<std::size_t>(SketchElementKind::line)] = 1;
    CHECK(data.consistent());
    data.elements.push_back({"L1", SketchElementKind::line, segment(0, 0, 2, 0), false});
    CHECK_FALSE(data.consistent()); // repeated id
    data.elements.back().id = "C1"; // the kind and the letter disagree
    CHECK_FALSE(data.consistent());
    data.elements.back() = {"C1", SketchElementKind::circle, segment(0, 0, 2, 0), false};
    data.lastNumber[static_cast<std::size_t>(SketchElementKind::circle)] = 1;
    CHECK(data.consistent());
    data.rules.push_back({"R1", "", {}});
    data.rules.push_back({"R1", "", {}});
    CHECK_FALSE(data.consistent());
}

TEST_CASE("sketch data: the fingerprint changes with every number that matters", "[sketch]") {
    SketchData data;
    data.lastNumber[static_cast<std::size_t>(SketchElementKind::line)] = 1;
    data.elements.push_back({"L1", SketchElementKind::line, segment(0, 0, 1, 0), false});
    const std::string base = data.fingerprint();
    SketchData moved = data;
    moved.elements[0].points[1].u = 1.0000001;
    CHECK(moved.fingerprint() != base);
    SketchData aid = data;
    aid.elements[0].construction = true;
    CHECK(aid.fingerprint() != base);
    CHECK(SketchData(data).fingerprint() == base);
}

// --- in the document ---------------------------------------------------------------------

TEST_CASE("a sketch step has a sketch, makes no body and keeps its number", "[sketch][document]") {
    Document document(3);
    const auto step = document.addFeature(sketchDraft());
    REQUIRE(step);
    CHECK(document.findFeature(*step)->sketch.has_value());
    CHECK(document.bodies().empty());
    CHECK(document.findFeature(*step)->number == 1);

    FeatureDraft broken = sketchDraft();
    broken.sketch->elements.push_back({"L1", SketchElementKind::line, segment(0, 0, 1, 1), false});
    CHECK_FALSE(document.addFeature(broken)); // counter behind the id: refused
}

TEST_CASE("document: sketch elements get ids that are never given out twice",
          "[sketch][document]") {
    Document document(3);
    const FeatureId step = *document.addFeature(sketchDraft());
    const auto first =
        document.addSketchElement(step, SketchElementKind::line, segment(0, 0, 5, 0));
    const auto second =
        document.addSketchElement(step, SketchElementKind::line, segment(0, 0, 0, 5));
    const auto circle =
        document.addSketchElement(step, SketchElementKind::circle, segment(1, 1, 3, 1));
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(circle);
    CHECK(*first == "L1");
    CHECK(*second == "L2");
    CHECK(*circle == "C1");

    REQUIRE(document.takeSketchElement(step, "L1"));
    CHECK(*document.addSketchElement(step, SketchElementKind::line, segment(1, 1, 2, 2)) == "L3");
    const SketchData& sketch = *document.findFeature(step)->sketch;
    CHECK(sketch.elements.size() == 3);
    CHECK(sketch.consistent());
}

TEST_CASE("document: sketch changes are refused when they make no sense", "[sketch][document]") {
    Document document(3);
    const FeatureId step = *document.addFeature(sketchDraft());
    const FeatureId box = *document.addFeature([] {
        FeatureDraft draft;
        draft.type = "box";
        draft.newBodies = 1;
        return draft;
    }());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(document.addSketchElement(box, SketchElementKind::line, segment(0, 0, 1, 1)));
    CHECK_FALSE(
        document.addSketchElement(FeatureId{"nope"}, SketchElementKind::line, segment(0, 0, 1, 1)));
    CHECK_FALSE(document.addSketchElement(step, SketchElementKind::line, {{0, 0}}));
    CHECK_FALSE(document.addSketchElement(step, SketchElementKind::line, segment(0, 0, nan, 1)));
    CHECK(document.findFeature(step)->sketch->elements.empty());
    CHECK(document.findFeature(step)->sketch->lastNumber[1] == 0); // no number was used

    document.addSketchElement(step, SketchElementKind::line, segment(0, 0, 1, 1));
    CHECK_FALSE(document.setSketchPoints(step, "L1", {{0, 0}, {1, 1}, {2, 2}})); // a line has two
    CHECK_FALSE(document.setSketchPoints(step, "L1", segment(0, 0, nan, 1)));
    CHECK_FALSE(document.setSketchPoints(step, "L9", segment(0, 0, 1, 1)));
    CHECK_FALSE(document.setSketchConstruction(step, "L9", true));
    CHECK_FALSE(document.takeSketchElement(step, "L9"));
}

TEST_CASE("document: a spline takes more or fewer points, a line never", "[sketch][document]") {
    Document document(3);
    const FeatureId step = *document.addFeature(sketchDraft());
    document.addSketchElement(step, SketchElementKind::spline, {{0, 0}, {1, 1}});
    CHECK(document.setSketchPoints(step, "S1", {{0, 0}, {1, 2}, {2, 0}}));
    CHECK(document.findFeature(step)->sketch->elements[0].points.size() == 3);
    CHECK_FALSE(document.setSketchPoints(step, "S1", {{0, 0}}));
}

TEST_CASE("document: sketches count as content for sameContentAs", "[sketch][document]") {
    Document a(5);
    const FeatureId step = *a.addFeature(sketchDraft());
    Document b = a;
    CHECK(a.sameContentAs(b));
    b.addSketchElement(step, SketchElementKind::point, {{1, 2}});
    CHECK_FALSE(a.sameContentAs(b));
    const std::uint64_t before = b.revision();
    b.setSketchConstruction(step, "P1", true);
    CHECK(b.revision() > before);
}

// --- commands -----------------------------------------------------------------------------

TEST_CASE("sketch commands: add, move, mark as aid and remove, each with Undo and Redo",
          "[sketch][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    const Document start = history.document();

    auto add = std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line,
                                                         segment(0, 0, 10, 0));
    const AddSketchElementCommand* added = add.get();
    REQUIRE(history.execute(std::move(add)));
    REQUIRE(added->result());
    CHECK(*added->result() == "L1");
    const Document afterAdd = history.document();

    REQUIRE(history.execute(makeMoveSketchPoints(step, {{"L1", segment(0, 0, 10, 5)}})));
    CHECK(history.document().findFeature(step)->sketch->elements[0].points[1] ==
          SketchPoint{10, 5});
    const Document afterMove = history.document();

    REQUIRE(history.execute(makeSetSketchConstruction(step, "L1", true)));
    CHECK(history.document().findFeature(step)->sketch->elements[0].construction);
    const Document afterAid = history.document();

    REQUIRE(history.execute(makeRemoveSketchElement(step, "L1")));
    CHECK(history.document().findFeature(step)->sketch->elements.empty());
    CHECK(history.document().findFeature(step)->sketch->lastNumber[1] == 1);

    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(afterAid));
    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(afterMove));
    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(afterAdd));
    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(start)); // the number L1 is free again
    REQUIRE(history.redo());
    CHECK(history.document().sameContentAs(afterAdd)); // the same id comes back
}

TEST_CASE("sketch commands: nothing is changed by a refused or empty command",
          "[sketch][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line,
                                                              segment(0, 0, 10, 0)));
    const Document before = history.document();
    const std::size_t steps = history.position();

    CHECK_FALSE(history.execute(makeRemoveSketchElement(step, "L7")));
    CHECK_FALSE(history.execute(makeMoveSketchPoints(step, {{"L7", segment(0, 0, 1, 1)}})));
    CHECK_FALSE(history.execute(makeMoveSketchPoints(step, {})));
    CHECK_FALSE(
        history.execute(makeMoveSketchPoints(FeatureId{"nope"}, {{"L1", segment(0, 0, 1, 1)}})));
    // A move that is the same as the state is valid but records no step; a wrong number of points
    // is refused.
    history.execute(makeMoveSketchPoints(step, {{"L1", segment(0, 0, 10, 0)}}));
    CHECK_FALSE(history.execute(makeMoveSketchPoints(step, {{"L1", {{0, 0}}}})));
    history.execute(makeSetSketchConstruction(step, "L1", false)); // already so: no step
    CHECK_FALSE(history.execute(std::make_unique<AddSketchElementCommand>(
        step, SketchElementKind::circle, std::vector<SketchPoint>{{0, 0}})));
    CHECK(history.document().sameContentAs(before));
    CHECK(history.position() == steps);
}

TEST_CASE("sketch commands: a move of two elements is all or nothing", "[sketch][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line,
                                                              segment(0, 0, 10, 0)));
    history.execute(std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line,
                                                              segment(0, 1, 10, 1)));
    const Document before = history.document();
    // The second one has a wrong number of points: the first must not stay moved.
    CHECK_FALSE(history.execute(
        makeMoveSketchPoints(step, {{"L1", segment(5, 5, 6, 6)}, {"L2", {{0, 0}}}})));
    CHECK(history.document().sameContentAs(before));

    REQUIRE(history.execute(
        makeMoveSketchPoints(step, {{"L1", segment(5, 5, 6, 6)}, {"L2", segment(7, 7, 8, 8)}})));
    CHECK(history.steps().size() == 4);
    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
}

TEST_CASE("sketch commands: dragging a point is one Undo step", "[sketch][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(step, SketchElementKind::line,
                                                              segment(0, 0, 10, 0)));
    const Document before = history.document();
    const std::size_t position = history.position();

    history.beginTransaction({"moveSketchPoints", {}});
    for (int i = 1; i <= 50; ++i) {
        REQUIRE(history.execute(makeMoveSketchPoints(step, {{"L1", segment(0, 0, 10, i * 0.5)}})));
    }
    history.commitTransaction();
    CHECK(history.position() == position + 1);
    CHECK(history.document().findFeature(step)->sketch->elements[0].points[1] ==
          SketchPoint{10, 25});

    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
    CHECK(history.position() == position);
    REQUIRE(history.redo());
    CHECK(history.document().findFeature(step)->sketch->elements[0].points[1] ==
          SketchPoint{10, 25});
}

TEST_CASE("sketch commands: removing the sketch step and undoing it brings the elements back",
          "[sketch][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(step, SketchElementKind::circle,
                                                              segment(0, 0, 4, 0)));
    const Document before = history.document();
    REQUIRE(history.execute(makeRemoveFeature(step)));
    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
    CHECK(history.document().findFeature(step)->sketch->elements.size() == 1);
}

TEST_CASE("sketch commands describe themselves with keys and values, no text",
          "[sketch][history]") {
    History history{Document(9)};
    REQUIRE(history.addFeature(sketchDraft()));
    const FeatureId step = history.document().features().front().id;
    history.execute(std::make_unique<AddSketchElementCommand>(
        step, SketchElementKind::arc, std::vector<SketchPoint>{{0, 0}, {1, 0}, {0, 1}}));
    history.execute(makeSetSketchConstruction(step, "A1", true));
    history.execute(makeMoveSketchPoints(step, {{"A1", {{0, 0}, {2, 0}, {0, 2}}}}));
    history.execute(makeRemoveSketchElement(step, "A1"));
    const auto& steps = history.steps();
    REQUIRE(steps.size() == 5);
    CHECK(steps[1].key == "addSketchElement");
    CHECK(steps[1].values.at("kind") == "arc");
    CHECK(steps[1].values.at("element") == "A1");
    CHECK(steps[1].values.at("number") == "1");
    CHECK(steps[2].key == "sketchConstructionOn");
    CHECK(steps[3].key == "moveSketchPoints");
    CHECK(steps[3].values.at("count") == "1");
    CHECK(steps[4].key == "removeSketchElement");
    CHECK(steps[4].values.at("element") == "A1");
}
