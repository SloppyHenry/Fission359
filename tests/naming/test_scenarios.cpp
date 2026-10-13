// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Scenarios from AP-0.6: typical changes after which references have to keep pointing to the
// right place (or honestly say "needs help").

#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "scenario_support.h"

using namespace fission::test::naming;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST_CASE("naming: a box names its six faces and twelve edges", "[naming][basics]") {
    Recipe recipe{makeBoxFeature("F1", 40, 20, 10)};
    const Evaluation ev = evaluate(recipe);
    REQUIRE(ev.features[0].status == FeatureStatus::ok);
    REQUIRE(ev.body.faces().size() == 6);
    REQUIRE(ev.body.edges().size() == 12);
    for (const Entry& face : ev.body.faces()) {
        REQUIRE_FALSE(face.names.empty());
    }
    for (const Entry& edge : ev.body.edges()) {
        REQUIRE_FALSE(edge.names.empty());
    }
    REQUIRE(ev.body.find(Kind::face, "F1/box/face:+Z").size() == 1);
    REQUIRE(ev.body.find(Kind::edge, "edge[F1/box/face:+X|F1/box/face:+Z]").size() == 1);
}

TEST_CASE("naming: box, round one edge, then change the height", "[naming][scenario]") {
    BoxDims dims{40, 20, 10};
    Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
    const Body box = bodyAfter(recipe);
    const TopoDS_Shape edge = only(boxEdge(box.shape(), dims, Side::posX, Side::posZ));
    recipe.push_back(makeFilletFeature("F2", {makeReference(box, edge, Kind::edge)}, 2.0));

    SECTION("unchanged: the fillet is applied and the volume matches the formula") {
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        const double expected = 40 * 20 * 10 - (1.0 - kPi / 4.0) * 2.0 * 2.0 * 20;
        REQUIRE(volumeOf(ev.body.shape()) == Catch::Approx(expected).epsilon(1e-6));
        REQUIRE(ev.body.find(Kind::face, "F2/fillet[edge[F1/box/face:+X|F1/box/face:+Z]]").size() ==
                1);
    }

    SECTION("height, width and depth changes keep the reference") {
        const BoxDims changes[] = {
            {40, 20, 25}, {60, 20, 10}, {40, 35, 10}, {10, 8, 5}, {40, 20, 4}};
        for (const BoxDims& changed : changes) {
            Recipe edited = recipe;
            edited[0].dx = changed.dx;
            edited[0].dy = changed.dy;
            edited[0].dz = changed.dz;
            const Evaluation ev = evaluate(edited);
            REQUIRE(ev.features[1].status == FeatureStatus::ok);
            const truth::Shapes expected =
                boxEdge(ev.features[1].input.shape(), changed, Side::posX, Side::posZ);
            const Verdict verdict =
                measure("box-fillet-height", ev.features[1].input, recipe[1].refs[0], expected);
            REQUIRE(verdict == Verdict::correct);
            const double volume = 40.0 * 0.0 + changed.dx * changed.dy * changed.dz -
                                  (1.0 - kPi / 4.0) * 4.0 * changed.dy;
            REQUIRE(volumeOf(ev.body.shape()) == Catch::Approx(volume).epsilon(1e-6));
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Sketch -> extrude -> drill into a side face -> extend the sketch by one segment

namespace {

struct ExtrudeHole {
    Recipe recipe;
    std::vector<SketchPoint> sketch;
    double height = 10.0;
};

// F1: rectangle 40 x 20 extruded by 10. F2: blind hole of radius 3 into the right side (S1).
ExtrudeHole makeExtrudeHole() {
    ExtrudeHole m;
    m.sketch = rectangleSketch(40, 20);
    m.recipe.push_back(makeExtrudeFeature("F1", m.sketch, m.height));
    const Body body = bodyAfter(m.recipe);
    const TopoDS_Shape side = only(segmentFace(body.shape(), m.sketch, 1));
    m.recipe.push_back(
        makeHoleFeature("F2", makeReference(body, side, Kind::face), gp_Pnt(40, 10, 5), 3.0, 15.0));
    return m;
}

} // namespace

TEST_CASE("naming: sketch, extrude, drill into a side, extend the sketch", "[naming][scenario]") {
    ExtrudeHole m = makeExtrudeHole();

    SECTION("the hole is cut into the right side wall") {
        const Evaluation ev = evaluate(m.recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        const double expected = 40.0 * 20.0 * 10.0 - kPi * 9.0 * 15.0;
        REQUIRE(volumeOf(ev.body.shape()) == Catch::Approx(expected).epsilon(1e-6));
        REQUIRE(ev.body.find(Kind::face, "F2/hole/wall").size() == 1);
        REQUIRE(ev.body.find(Kind::face, "F2/hole/floor").size() == 1);
    }

    SECTION("a new point in the top segment: the drilled side keeps its reference") {
        std::vector<SketchPoint> sketch = m.sketch;
        insertPoint(sketch, 2, 20, 30, 4);
        m.recipe[0].sketch = sketch;
        const Reference saved = m.recipe[1].refs[0];
        const Evaluation ev = evaluate(m.recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        const truth::Shapes expected = segmentFace(ev.features[1].input.shape(), sketch, 1);
        REQUIRE(measure("extrude-hole-extend-sketch", ev.features[1].input, saved, expected) ==
                Verdict::correct);
        REQUIRE(ev.features[1].refs[0].outcome == Outcome::found);
    }

    SECTION("a new point in the drilled segment itself: it keeps the same face, bent") {
        std::vector<SketchPoint> sketch = m.sketch;
        insertPoint(sketch, 1, 46, 10, 4); // S1 now runs (40,0) -> (46,10)
        m.recipe[0].sketch = sketch;
        const Reference saved = m.recipe[1].refs[0];
        const Evaluation ev = evaluate(m.recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        const truth::Shapes expected = segmentFace(ev.features[1].input.shape(), sketch, 1);
        const Verdict verdict =
            measure("extrude-hole-extend-sketch", ev.features[1].input, saved, expected);
        REQUIRE(verdict == Verdict::correct);
    }
}

TEST_CASE("naming: insert a feature before an existing one", "[naming][scenario]") {
    const BoxDims dims{40, 20, 10};
    Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
    const Body box = bodyAfter(recipe);
    const TopoDS_Shape edge = only(boxEdge(box.shape(), dims, Side::posX, Side::posZ));
    recipe.push_back(makeFilletFeature("F2", {makeReference(box, edge, Kind::edge)}, 2.0));
    const Reference saved = recipe[1].refs[0];

    SECTION("a hole inserted in front of the fillet") {
        const TopoDS_Shape top = only(boxFace(box.shape(), dims, Side::posZ));
        recipe.insert(recipe.begin() + 1, makeHoleFeature("F3", makeReference(box, top, Kind::face),
                                                          gp_Pnt(15, 10, 10), 4.0));
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(ev.features[2].status == FeatureStatus::ok);
        const truth::Shapes expected =
            boxEdge(ev.features[2].input.shape(), dims, Side::posX, Side::posZ);
        REQUIRE(measure("insert-feature-before", ev.features[2].input, saved, expected) ==
                Verdict::correct);
    }

    SECTION("another fillet on the neighbouring edge inserted in front") {
        const TopoDS_Shape other = only(boxEdge(box.shape(), dims, Side::posX, Side::posY));
        recipe.insert(recipe.begin() + 1,
                      makeFilletFeature("F3", {makeReference(box, other, Kind::edge)}, 3.0));
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(ev.features[2].status == FeatureStatus::ok);
        const truth::Shapes expected =
            boxEdge(ev.features[2].input.shape(), dims, Side::posX, Side::posZ);
        REQUIRE(measure("insert-feature-before", ev.features[2].input, saved, expected) ==
                Verdict::correct);
    }

    SECTION("a boss on the opposite side inserted in front") {
        recipe.insert(recipe.begin() + 1,
                      makeBoxFeature("F3", 10, 10, 5, gp_Pnt(5, 5, -5))); // fused under the base
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(ev.features[2].status == FeatureStatus::ok);
        const truth::Shapes expected =
            boxEdge(ev.features[2].input.shape(), dims, Side::posX, Side::posZ);
        REQUIRE(measure("insert-feature-before", ev.features[2].input, saved, expected) ==
                Verdict::correct);
    }
}

TEST_CASE("naming: rim of a hole survives moving the hole and resizing the plate",
          "[naming][scenario]") {
    BoxDims dims{60, 30, 10};
    Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
    Body body = bodyAfter(recipe);
    const TopoDS_Shape top = only(boxFace(body.shape(), dims, Side::posZ));
    recipe.push_back(
        makeHoleFeature("F2", makeReference(body, top, Kind::face), gp_Pnt(20, 15, 10), 4.0));
    body = bodyAfter(recipe);
    const TopoDS_Shape rim =
        only(truth::circleEdges(body.shape(), gp_Pnt(20, 15, 10), gp_Dir(0, 0, 1), 4.0));
    recipe.push_back(makeFilletFeature("F3", {makeReference(body, rim, Kind::edge)}, 1.0));
    const Reference saved = recipe[2].refs[0];

    REQUIRE(evaluate(recipe).features[2].status == FeatureStatus::ok);

    SECTION("move the hole, make the hole bigger, make the plate thicker") {
        struct Edit {
            double x, y, r, height;
        };
        for (const Edit& edit :
             {Edit{35, 15, 4, 10}, Edit{20, 15, 6, 10}, Edit{20, 15, 4, 18}, Edit{45, 20, 5, 7}}) {
            Recipe edited = recipe;
            edited[0].dz = edit.height;
            edited[1].center = gp_Pnt(edit.x, edit.y, edit.height);
            edited[1].radius = edit.r;
            const Evaluation ev = evaluate(edited);
            REQUIRE(ev.features[2].status == FeatureStatus::ok);
            const truth::Shapes expected =
                truth::circleEdges(ev.features[2].input.shape(),
                                   gp_Pnt(edit.x, edit.y, edit.height), gp_Dir(0, 0, 1), edit.r);
            REQUIRE(measure("hole-rim-fillet", ev.features[2].input, saved, expected) ==
                    Verdict::correct);
        }
    }
}

TEST_CASE("naming: a replaced feature is found again by similarity", "[naming][scenario]") {
    ExtrudeHole m = makeExtrudeHole();
    const Reference saved = m.recipe[1].refs[0];
    REQUIRE(saved.name == "F1/extrude/side[S1]");

    // The user re-draws the outline: a new feature with new segment ids replaces F1.
    std::vector<SketchPoint> redrawn = rectangleSketch(40, 20, 10);
    m.recipe[0] = makeExtrudeFeature("F9", redrawn, 10.0);
    const Evaluation ev = evaluate(m.recipe);
    REQUIRE(ev.features[1].status == FeatureStatus::ok);
    REQUIRE(ev.features[1].refs[0].outcome == Outcome::fallback);
    REQUIRE_FALSE(ev.features[1].refs[0].checkPlease);
    // The right face is the one standing on the segment that is now called S11.
    const truth::Shapes expected = segmentFace(ev.features[1].input.shape(), redrawn, 11);
    REQUIRE(measure("replaced-feature", ev.features[1].input, saved, expected) == Verdict::correct);
    // ... and the document keeps the new name from now on.
    REQUIRE(m.recipe[1].refs[0].name == "F9/extrude/side[S11]");
}
