// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// References in the recompute (AP-2.10b/c): a step with a reference gets the element found
// again, or is not calculated and shows its last valid result with a warning. Healing, the
// command that picks again, and the place of the creating step.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fission/core/commands.h"
#include "fission/core/history.h"

#include "refs_support.h"

using Catch::Approx;
using namespace fission::core;
using fission::test::RefWorld;

TEST_CASE("refs: a step gets the element its reference leads to", "[refs]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult first = world.run();
    const FeatureId mark =
        world.mark_(plate, world.pick(first, plate, ElementKind::face, "F1/box/top"));

    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(mark);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    REQUIRE(world.mark->seen.size() == 1);
    CHECK(world.mark->seen.front().element == "F1/box/top");
    CHECK(world.mark->seen.front().kind == ElementKind::face);
    CHECK(result.healed.empty());

    // Unchanged: nothing is calculated again and nothing needs healing.
    const RecomputeResult again = world.run();
    CHECK(again.executed == 0);
    CHECK(again.healed.empty());
}

TEST_CASE("refs: the reference follows a change of size, and its look is healed", "[refs]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult first = world.run();
    const FeatureId mark = world.mark_(
        plate, world.pick(first, plate, ElementKind::edge, "edge[F1/box/right|F1/box/top]"));
    world.run();

    REQUIRE(world.document.setFeatureValue(plate, "height", "25"));
    const RecomputeResult result = world.run();
    CHECK(result.state(mark)->status == FeatureStatus::ok);
    REQUIRE(world.mark->seen.size() == 1);
    CHECK(world.mark->seen.front().element == "edge[F1/box/right|F1/box/top]");

    // The saved look is out of date (the edge is higher now): healing brings it up to date.
    REQUIRE(result.healed.size() == 1);
    CHECK(result.healed.front().feature == mark);
    CHECK(result.healed.front().after.look.centroid.z == Approx(25.0));
    CHECK(result.healed.front().before.look.centroid.z == Approx(10.0));
    CHECK(applyHealedRefs(world.document, result) == 1);
    CHECK(world.document.findFeature(mark)->refs.at("target").front().look.centroid.z ==
          Approx(25.0));

    // Once healed, nothing is left to heal.
    CHECK(world.run().healed.empty());
}

TEST_CASE("refs: a healed look that was changed meanwhile is not written", "[refs]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult first = world.run();
    const FeatureId mark =
        world.mark_(plate, world.pick(first, plate, ElementKind::face, "F1/box/top"));
    REQUIRE(world.document.setFeatureValue(plate, "height", "25"));
    const RecomputeResult result = world.run();
    REQUIRE(result.healed.size() == 1);

    // The person picked something else while the calculation ran.
    PersistentRef other = world.pick(first, plate, ElementKind::face, "F1/box/left");
    REQUIRE(world.document.setFeatureRefs(mark, "target", {other}));
    CHECK(applyHealedRefs(world.document, result) == 0);
    CHECK(world.document.findFeature(mark)->refs.at("target").front() == other);
}

TEST_CASE("refs: a reference that needs help stops the step and keeps its last result",
          "[refs][evil]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const FeatureId slot = world.add(
        "box", {{"width", "10"}, {"depth", "60"}, {"height", "4"}, {"x", "0"}, {"z", "5"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(plate), world.bodyOf(slot)}, {{"mode", "subtract"}});
    const RecomputeResult first = world.run();
    const FeatureId mark =
        world.mark_(cut, world.pick(first, cut, ElementKind::face, "F1/box/top"));
    REQUIRE(world.run().state(mark)->status == FeatureStatus::ok);

    // The slot grows and cuts the top in two: the reference cannot be kept safely.
    REQUIRE(world.document.setFeatureValue(slot, "height", "20"));
    const RecomputeResult broken = world.run();
    const FeatureState* state = broken.state(mark);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::warning);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::referenceNeedsHelp);
    CHECK(state->issue->helpReason == HelpReason::split);
    CHECK(state->issue->subject == "target");
    CHECK(state->stale);
    CHECK(broken.bodies.count(world.bodyOf(mark)) == 1); // the last valid result stays visible
    CHECK(broken.bodies.at(world.bodyOf(mark)).stale);
    CHECK(broken.healed.empty());

    // The slot is small again: the reference is found, the warning is gone.
    REQUIRE(world.document.setFeatureValue(slot, "height", "4"));
    const RecomputeResult mended = world.run();
    CHECK(mended.state(mark)->status == FeatureStatus::ok);
    CHECK_FALSE(mended.state(mark)->stale);
}

TEST_CASE("refs: a step without an input body cannot find its reference", "[refs][evil]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult first = world.run();
    const PersistentRef ref = world.pick(first, plate, ElementKind::face, "F1/box/top");
    FeatureDraft draft = defaultDraft(*world.registry.find("markFace"));
    draft.refs["target"] = {ref}; // no input
    const FeatureId lonely = *world.document.addFeature(std::move(draft));
    const RecomputeResult result = world.run();
    REQUIRE(result.state(lonely)->issue.has_value());
    CHECK(result.state(lonely)->issue->code == FeatureIssueCode::referenceNeedsHelp);
    CHECK(result.state(lonely)->issue->helpReason == HelpReason::notFound);
}

TEST_CASE("refs: turning the creating step does not make its faces look changed",
          "[refs][placement]") {
    RefWorld world;
    const FeatureId plate = world.plate("40", "20", "10");
    const RecomputeResult first = world.run();
    const FeatureId mark =
        world.mark_(plate, world.pick(first, plate, ElementKind::face, "F1/box/right"));
    REQUIRE(world.run().state(mark)->status == FeatureStatus::ok);

    for (const char* turn : {"90", "180", "37", "0"}) {
        REQUIRE(world.document.setFeatureValue(plate, "rotZ", turn));
        const RecomputeResult result = world.run();
        INFO("turned by " << turn);
        CHECK(result.state(mark)->status == FeatureStatus::ok);
        REQUIRE(world.mark->seen.size() == 1);
        CHECK(world.mark->seen.front().element == "F1/box/right");
    }
}

TEST_CASE("refs: picking again is one undo step", "[refs][command]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult first = world.run();
    const PersistentRef top = world.pick(first, plate, ElementKind::face, "F1/box/top");
    const PersistentRef left = world.pick(first, plate, ElementKind::face, "F1/box/left");
    const FeatureId mark = world.mark_(plate, top);

    History history(world.document);
    REQUIRE(history.execute(makeSetFeatureRefs(mark, "target", {left})));
    CHECK(history.document().findFeature(mark)->refs.at("target").front() == left);
    REQUIRE(history.undo());
    CHECK(history.document().findFeature(mark)->refs.at("target").front() == top);
    REQUIRE(history.redo());
    CHECK(history.document().findFeature(mark)->refs.at("target").front() == left);

    // The same pick again changes nothing and leaves no step.
    const std::size_t steps = history.steps().size();
    REQUIRE(history.execute(makeSetFeatureRefs(mark, "target", {left})));
    CHECK(history.steps().size() == steps);
    // A step that does not exist refuses.
    CHECK_FALSE(history.execute(makeSetFeatureRefs(FeatureId{"nope"}, "target", {left})));
}

TEST_CASE("refs: mending through the history leaves no undo step", "[refs]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult first = world.run();
    const FeatureId mark =
        world.mark_(plate, world.pick(first, plate, ElementKind::face, "F1/box/top"));
    History history(world.document);
    REQUIRE(history.execute(makeSetFeatureValue(plate, "height", "25")));
    const std::size_t steps = history.steps().size();

    auto result = recompute(history.document(), world.registry, *world.kernel, world.cache);
    REQUIRE(result.has_value());
    CHECK(history.mendRefs(*result) == 1);
    CHECK(history.steps().size() == steps);
    CHECK(history.document().findFeature(mark)->refs.at("target").front().look.centroid.z ==
          Approx(25.0));
}

TEST_CASE("refs: makeRef refuses what cannot be found again", "[refs]") {
    RefWorld world;
    const FeatureId plate = world.plate();
    const RecomputeResult result = world.run();
    const BodyLooks looks = world.looksOf(result, plate);
    CHECK_FALSE(makeRef(looks, ElementKind::face, 999, {}).has_value());
    BodyLooks unnamed = looks;
    unnamed.faces.front().name.clear();
    CHECK_FALSE(makeRef(unnamed, ElementKind::face, unnamed.faces.front().id, {}).has_value());
    const auto ref = makeRef(looks, ElementKind::face, looks.faces.front().id, {});
    REQUIRE(ref.has_value());
    CHECK(ref->name == looks.faces.front().name);
}

TEST_CASE("refs: the place of a step comes from its turn", "[refs][placement]") {
    RefWorld world;
    const FeatureId plate = world.plate("40", "20", "10");
    REQUIRE(world.document.setFeatureValue(plate, "rotZ", "90"));
    const RecomputeResult result = world.run();
    const Placement turned =
        placementOf(world.document, world.registry, result.features, "F1/box/top");
    CHECK(turned.angles.z == Approx(90.0));
    CHECK(turned.centre.z == Approx(5.0)); // the middle of the box, 10 mm high
    // Edges are named after their faces: the same place.
    CHECK(placementOf(world.document, world.registry, result.features,
                      "edge[F1/box/right|F1/box/top]") == turned);
    // Not a name of a step, or a step that is not there: no place.
    CHECK(placementOf(world.document, world.registry, result.features, "other") == Placement{});
    CHECK(placementOf(world.document, world.registry, result.features, "F77/box/top") ==
          Placement{});
}
