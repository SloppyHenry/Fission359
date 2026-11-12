// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/document.h"

using namespace fission::core;

namespace {

FeatureDraft boxDraft(const char* width = "20") {
    FeatureDraft draft;
    draft.type = "box";
    draft.params["width"] = {width, QuantityKind::length};
    draft.newBodies = 1;
    return draft;
}

std::vector<FeatureId> orderOf(const Document& document) {
    std::vector<FeatureId> ids;
    for (const Feature& feature : document.features()) {
        ids.push_back(feature.id);
    }
    return ids;
}

} // namespace

TEST_CASE("an independent step moves to a new place, the command undoes it", "[core][move]") {
    Document document(11);
    const auto a = document.addFeature(boxDraft());
    const auto b = document.addFeature(boxDraft());
    const auto c = document.addFeature(boxDraft());
    REQUIRE(document.features().size() == 3);

    auto command = makeMoveFeature(*c, 0);
    REQUIRE(command->apply(document) == ApplyResult::applied);
    CHECK(orderOf(document) == std::vector<FeatureId>{*c, *a, *b});
    command->revert(document);
    CHECK(orderOf(document) == std::vector<FeatureId>{*a, *b, *c});

    // Moving to the index the step already has succeeds and changes nothing.
    const auto revision = document.revision();
    CHECK(document.moveFeature(*c, 2));
    CHECK(document.revision() == revision);
    CHECK(document.moveFeature(*c, 0));
    CHECK(orderOf(document) == std::vector<FeatureId>{*c, *a, *b});
}

TEST_CASE("a step cannot move in front of the step that makes its input body", "[core][move]") {
    Document document(12);
    const auto a = document.addFeature(boxDraft());
    const BodyId bodyA = document.findFeature(*a)->outputs.at(0);

    FeatureDraft uses;
    uses.type = "move";
    uses.inputs = {bodyA};
    const auto b = document.addFeature(uses);
    const auto c = document.addFeature(boxDraft());
    REQUIRE(orderOf(document) == std::vector<FeatureId>{*a, *b, *c});

    // B in front of A would work on a body that is only made behind it.
    CHECK_FALSE(document.moveFeature(*b, 0));
    // A behind B: the same dependency, seen from the other side.
    CHECK_FALSE(document.moveFeature(*a, 1));
    CHECK_FALSE(document.moveFeature(*a, 2));
    CHECK(orderOf(document) == std::vector<FeatureId>{*a, *b, *c});

    // The independent C moves freely.
    CHECK(document.moveFeature(*c, 0));
    CHECK(orderOf(document) == std::vector<FeatureId>{*c, *a, *b});
}

TEST_CASE("a reference to another step keeps that step in front of it", "[core][move]") {
    Document document(13);
    const auto a = document.addFeature(boxDraft());
    const int numberA = document.findFeature(*a)->number;
    const auto b = document.addFeature(boxDraft());

    PersistentRef ref;
    ref.name = "F" + std::to_string(numberA) + "/box/top";
    REQUIRE(document.setFeatureRefs(*b, "face", {ref}));
    CHECK_FALSE(document.moveFeature(*b, 0)); // would point to a later step
    CHECK(orderOf(document) == std::vector<FeatureId>{*a, *b});

    // A name that is no "F<n>/..." reference is ignored.
    PersistentRef other;
    other.name = "top";
    REQUIRE(document.setFeatureRefs(*b, "face", {other}));
    CHECK(document.moveFeature(*b, 0));
    CHECK(orderOf(document) == std::vector<FeatureId>{*b, *a});
}

TEST_CASE("a place beyond the recipe and an unknown step are refused", "[core][move]") {
    Document document(14);
    const auto a = document.addFeature(boxDraft());
    const auto b = document.addFeature(boxDraft());

    CHECK_FALSE(document.moveFeature(*a, 2)); // only two steps
    CHECK_FALSE(document.moveFeature(FeatureId{"nope"}, 0));
    auto command = makeMoveFeature(*a, 5);
    CHECK(command->apply(document) == ApplyResult::refused);
    CHECK(orderOf(document) == std::vector<FeatureId>{*a, *b});
}

TEST_CASE("the time marker keeps its count when a step moves", "[core][move]") {
    Document document(15);
    const auto a = document.addFeature(boxDraft());
    const auto b = document.addFeature(boxDraft());
    const auto c = document.addFeature(boxDraft());
    REQUIRE(document.setRollback(2)); // the first two steps count
    CHECK(document.rollback() == 2);

    CHECK(document.moveFeature(*c, 0));
    CHECK(document.rollback() == 2);
    CHECK(orderOf(document) == std::vector<FeatureId>{*c, *a, *b});
}

TEST_CASE("a link that is already broken blocks no move", "[core][move]") {
    Document document(16);
    const auto a = document.addFeature(boxDraft());
    const BodyId bodyA = document.findFeature(*a)->outputs.at(0);
    FeatureDraft uses;
    uses.type = "fillet";
    uses.inputs = {bodyA};
    uses.newBodies = 0;
    uses.refs["edges"] = {PersistentRef{ElementKind::edge, "F1/box/top", {}}};
    const auto b = document.addFeature(uses);
    const auto c = document.addFeature(boxDraft());
    REQUIRE(b);
    REQUIRE(c);
    // The step that made the body is gone: b reports a missing input and an unknown edge.
    REQUIRE(document.removeFeature(*a));
    CHECK(document.moveFeature(*c, 0));
    CHECK(orderOf(document) == std::vector<FeatureId>{*c, *b});
    CHECK(document.moveFeature(*b, 0));
    CHECK(orderOf(document) == std::vector<FeatureId>{*b, *c});
}
