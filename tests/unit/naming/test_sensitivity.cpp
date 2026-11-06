// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// "Switch a building block off and the suite turns red" (AP-2.10d, ADR-0003): for each safety
// measure of the resolution one case that proves it is needed. With the block on, the answer is
// "needs help"; with it off, the reference silently lands on a look-alike. If one of these tests
// fails, the suite has lost its grip on that block.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "judge.h"

using namespace fission::core;
using namespace fission::test;
namespace geo = fission::geo;

namespace {

ElementLook flatFace(double x, double y, double z, geo::Point3d direction) {
    ElementLook look;
    look.kind = ElementKind::face;
    look.shape = ElementShape::plane;
    look.centroid = {x, y, z};
    look.direction = direction;
    look.hasDirection = true;
    look.directed = true;
    look.size = 100.0;
    return look;
}

} // namespace

TEST_CASE("naming-product: sensitivity: the memory of used-up elements is needed",
          "[naming-product][sensitivity]") {
    // A thin box on top covers the top; its new top is half a millimetre higher and looks
    // almost the same. Only the memory of the old top tells the two apart.
    RefWorld world;
    const FeatureId plate = world.plate("40", "30", "10");
    const FeatureId cover = world.add(
        "box", {{"width", "40"}, {"depth", "30"}, {"height", "0.5"}, {"x", "0"}, {"z", "100"}});
    const FeatureId merged =
        world.add("combine", {}, {world.bodyOf(plate), world.bodyOf(cover)}, {{"mode", "merge"}});
    const RecomputeResult before = world.run();
    const PersistentRef top = world.pick(before, merged, ElementKind::face, "F1/box/top");
    REQUIRE(world.document.setFeatureValue(cover, "z", "10"));
    const RecomputeResult after = world.run();

    const BodyResult& body = after.bodies.at(world.bodyOf(merged));
    const BodyLooks looks = world.looksOf(after, merged);
    const PlacementFn place = [&](const std::string& name) {
        return placementOf(world.document, world.registry, after.features, name);
    };

    const ResolveOutcome safe = resolve(top, looks, body.consumed, place);
    REQUIRE(safe.state == ResolveOutcome::State::needsHelp);
    CHECK(safe.reason == HelpReason::consumed);

    ResolveSwitches off;
    off.useConsumed = false;
    const ResolveOutcome wrong = resolve(top, looks, body.consumed, place, {}, off);
    REQUIRE(wrong.state == ResolveOutcome::State::found); // silently the new top
    CHECK(wrong.element == "F2/box/top");
}

TEST_CASE("naming-product: sensitivity: the check for swapped places is needed",
          "[naming-product][sensitivity]") {
    const auto bodyWith = [](const std::string& left, const std::string& right) {
        BodyLooks looks;
        looks.diagonal = 100.0;
        looks.faces.push_back({1, left, flatFace(-20, 0, 0, {0, 0, 1})});
        looks.faces.push_back({2, right, flatFace(20, 0, 0, {0, 0, 1})});
        return looks;
    };
    const BodyLooks before = bodyWith("A", "B");
    const PersistentRef ref{ElementKind::face, "A", before.faces[0].look};
    const BodyLooks swapped = bodyWith("B", "A");

    CHECK(resolve(ref, swapped, {}, Placement{}).state == ResolveOutcome::State::needsHelp);
    ResolveSwitches off;
    off.checkSwap = false;
    CHECK(resolve(ref, swapped, {}, Placement{}, {}, off).state ==
          ResolveOutcome::State::found); // silently the wrong face
}

TEST_CASE("naming-product: sensitivity: the 45 degree rule is needed",
          "[naming-product][sensitivity]") {
    BodyLooks before;
    before.diagonal = 100.0;
    before.faces.push_back({1, "S", flatFace(0, 0, 0, {1, 0, 0})});
    const PersistentRef ref{ElementKind::face, "S", before.faces[0].look};
    BodyLooks turned;
    turned.diagonal = 100.0;
    turned.faces.push_back({1, "S", flatFace(0, 0, 0, {0.5, 0.866025403784, 0})}); // 60 degrees

    CHECK(resolve(ref, turned, {}, Placement{}).state == ResolveOutcome::State::needsHelp);
    ResolveSwitches off;
    off.checkTurn = false;
    CHECK(resolve(ref, turned, {}, Placement{}, {}, off).state == ResolveOutcome::State::found);
}
