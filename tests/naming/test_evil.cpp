// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// "Evil cases" of AP-0.6: situations in which guessing is dangerous. The only right answer is
// "needs help". A silent assignment here is exactly the stale-reference bug of other CAD
// programs, so these tests must never be weakened.

#include <cstdio>
#include <cstdlib>

#include "scenario_support.h"

using namespace fission::test::naming;

namespace {

// The strict system must ask for help. The lenient one is only measured (it may guess, but
// then only with "please check"; a guess without warning would be a failure there as well).
void expectHelp(const std::string& group, const Body& body, const Reference& saved) {
    const Resolution strict = resolve(body, saved, Strictness::strict);
    INFO("strict reason: " << strict.reason);
    REQUIRE(strict.outcome == Outcome::needsHelp);
    record(group, Strictness::strict, strict, {});

    const Resolution lenient = resolve(body, saved, Strictness::lenient);
    INFO("lenient reason: " << lenient.reason);
    const Verdict verdict = record(group, Strictness::lenient, lenient, {});
    if (std::getenv("FISSION_NAMING_VERBOSE") != nullptr) {
        std::fprintf(stderr, "[%s] lenient: %s (%s)\n", group.c_str(), toString(verdict),
                     lenient.reason.c_str());
    }
    REQUIRE(verdict != Verdict::silentlyWrong);
}

} // namespace

TEST_CASE("evil: a face is split in two", "[naming][evil]") {
    const BoxDims dims{40, 20, 10};
    Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
    const Body box = bodyAfter(recipe);
    const TopoDS_Shape top = only(boxFace(box.shape(), dims, Side::posZ));
    // F2 drills into the top face; its reference is the one under test.
    recipe.push_back(
        makeHoleFeature("F2", makeReference(box, top, Kind::face), gp_Pnt(5, 10, 10), 2.0));
    const Reference saved = recipe[1].refs[0];

    SECTION("a through hole wider than the plate cuts the top face into two pieces") {
        const Reference topRef = saved;
        recipe.insert(recipe.begin() + 1,
                      makeHoleFeature("F3", topRef, gp_Pnt(20, 10, 10), 14.0)); // 28 > 20 wide
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(boxFace(ev.features[2].input.shape(), dims, Side::posZ).size() == 2);
        expectHelp("evil-face-splits", ev.features[2].input, saved);
        REQUIRE(ev.features[2].status == FeatureStatus::needsHelp);
    }

    SECTION("a big hole that leaves one big piece and small slivers") {
        // Radius 11 at x = 30: the plate is 20 wide, so the hole cuts through. It leaves one
        // long piece on the left and two small corner slivers on the right.
        recipe.insert(recipe.begin() + 1, makeHoleFeature("F3", saved, gp_Pnt(30, 10, 10), 11.0));
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(boxFace(ev.features[2].input.shape(), dims, Side::posZ).size() >= 2);
        expectHelp("evil-face-splits", ev.features[2].input, saved);
    }
}

TEST_CASE("evil: a face disappears", "[naming][evil]") {
    SECTION("a sketch point is removed, the side face of its segment is gone") {
        std::vector<SketchPoint> sketch = rectangleSketch(40, 20);
        Recipe recipe{makeExtrudeFeature("F1", sketch, 10.0)};
        const Body body = bodyAfter(recipe);
        const TopoDS_Shape edge = only(segmentTopEdge(body.shape(), sketch, 2, 10.0));
        recipe.push_back(makeFilletFeature("F2", {makeReference(body, edge, Kind::edge)}, 1.5));
        const Reference saved = recipe[1].refs[0];

        removePoint(sketch, 2); // S1 now runs (40,0) -> (0,20); S2 no longer exists
        recipe[0].sketch = sketch;
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[0].status == FeatureStatus::ok);
        REQUIRE(segmentFace(ev.features[1].input.shape(), sketch, 2).empty());
        expectHelp("evil-face-vanishes", ev.features[1].input, saved);
        REQUIRE(ev.features[1].status == FeatureStatus::needsHelp);
    }

    SECTION("a boss with the same footprint swallows the top face") {
        const BoxDims dims{40, 20, 10};
        Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
        const Body base = bodyAfter(recipe);
        const TopoDS_Shape top = only(boxFace(base.shape(), dims, Side::posZ));
        const Reference saved = makeReference(base, top, Kind::face);
        recipe.push_back(makeBoxFeature("F2", 40, 20, 10, gp_Pnt(0, 0, 10)));
        recipe.push_back(makeHoleFeature("F3", saved, gp_Pnt(10, 10, 10), 3.0));
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(boxFace(ev.features[2].input.shape(), dims, Side::posZ).empty());
        expectHelp("evil-face-vanishes", ev.features[2].input, saved);
    }

    SECTION("the hole that made the referenced rim is deleted; an equal hole is near") {
        const BoxDims dims{60, 30, 10};
        Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
        Body body = bodyAfter(recipe);
        const Reference topRef =
            makeReference(body, only(boxFace(body.shape(), dims, Side::posZ)), Kind::face);
        recipe.push_back(makeHoleFeature("F2", topRef, gp_Pnt(20, 15, 10), 4.0));
        recipe.push_back(makeHoleFeature("F3", topRef, gp_Pnt(30, 15, 10), 4.0));
        body = bodyAfter(recipe);
        const TopoDS_Shape rim =
            only(truth::circleEdges(body.shape(), gp_Pnt(20, 15, 10), gp_Dir(0, 0, 1), 4.0));
        recipe.push_back(makeFilletFeature("F4", {makeReference(body, rim, Kind::edge)}, 0.8));
        const Reference saved = recipe[3].refs[0];

        recipe.erase(recipe.begin() + 1); // delete F2 (the hole the rim belongs to)
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[2].status == FeatureStatus::needsHelp);
        expectHelp("evil-face-vanishes", ev.features[2].input, saved);
    }
}

TEST_CASE("evil: two equal elements change places", "[naming][evil]") {
    SECTION("a mirrored sketch: the same segment id now stands on the opposite side") {
        std::vector<SketchPoint> sketch = rectangleSketch(40, 20);
        Recipe recipe{makeExtrudeFeature("F1", sketch, 10.0)};
        const Body body = bodyAfter(recipe);
        const TopoDS_Shape topSide = only(segmentFace(body.shape(), sketch, 2)); // y = 20
        recipe.push_back(makeHoleFeature("F2", makeReference(body, topSide, Kind::face),
                                         gp_Pnt(20, 20, 5), 3.0, 8.0));
        const Reference saved = recipe[1].refs[0];

        // Mirror at the line y = 10: S0 and S2 swap places, every normal in y flips.
        for (SketchPoint& point : sketch) {
            point.y = 20.0 - point.y;
        }
        recipe[0].sketch = sketch;
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[0].status == FeatureStatus::ok);
        expectHelp("evil-places-swap", ev.features[1].input, saved);
    }

    SECTION("two equal holes exchange their positions") {
        const BoxDims dims{60, 20, 10};
        Recipe recipe{makeBoxFeature("F1", dims.dx, dims.dy, dims.dz)};
        Body body = bodyAfter(recipe);
        const Reference topRef =
            makeReference(body, only(boxFace(body.shape(), dims, Side::posZ)), Kind::face);
        recipe.push_back(makeHoleFeature("F2", topRef, gp_Pnt(15, 10, 10), 4.0));
        recipe.push_back(makeHoleFeature("F3", topRef, gp_Pnt(45, 10, 10), 4.0));
        body = bodyAfter(recipe);
        const TopoDS_Shape rim =
            only(truth::circleEdges(body.shape(), gp_Pnt(15, 10, 10), gp_Dir(0, 0, 1), 4.0));
        recipe.push_back(makeFilletFeature("F4", {makeReference(body, rim, Kind::edge)}, 0.8));
        const Reference saved = recipe[3].refs[0];

        recipe[1].center = gp_Pnt(45, 10, 10);
        recipe[2].center = gp_Pnt(15, 10, 10);
        const Evaluation ev = evaluate(recipe);
        REQUIRE(ev.features[1].status == FeatureStatus::ok);
        REQUIRE(ev.features[2].status == FeatureStatus::ok);
        expectHelp("evil-places-swap", ev.features[3].input, saved);
    }
}
