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
