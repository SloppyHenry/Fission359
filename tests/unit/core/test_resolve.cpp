// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Resolving persistent references with the product code (AP-2.10b, ADR-0003): the scenarios of
// the prototype that the current features can build (box, cylinder, combine), with the same
// expectations. "Needs help" is always the right answer when guessing would be dangerous
// (rule 6): these tests must never be weakened. The scenarios with fillets, holes and sketches
// follow with the tools that make them (AP-2.10d).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "fission/core/body_looks.h"
#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/core/resolve.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::core;
namespace geo = fission::geo;

namespace {

struct World {
    Document document{5};
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;

    World() { fission::features::registerBuiltinFeatures(registry); }

    FeatureId add(const char* type, std::map<std::string, std::string> values = {},
                  std::vector<BodyId> inputs = {}, std::map<std::string, std::string> words = {}) {
        FeatureDraft draft = defaultDraft(*registry.find(type));
        for (const auto& [name, expression] : values) {
            draft.params.at(name).expression = expression;
        }
        for (const auto& [name, text] : words) {
            draft.texts[name] = text;
        }
        draft.inputs = std::move(inputs);
        return *document.addFeature(std::move(draft));
    }

    BodyId bodyOf(const FeatureId& id) const { return document.findFeature(id)->outputs.at(0); }

    FeatureId plate(const char* width = "40", const char* depth = "40", const char* height = "10") {
        return add("box", {{"width", width}, {"depth", depth}, {"height", height}, {"x", "0"}});
    }

    // The body made by `last`, with its looks and its memory of used-up elements.
    struct Seen {
        BodyLooks looks;
        std::vector<ConsumedElement> consumed;
    };
    Seen see(const FeatureId& last) {
        auto result = recompute(document, registry, *kernel, cache);
        REQUIRE(result.has_value());
        const BodyResult& body = result->bodies.at(bodyOf(last));
        return {describeBody(body.faces, kernel->signatures(body.solid)), body.consumed};
    }
};

const NamedLook& named(const BodyLooks& looks, ElementKind kind, const std::string& name) {
    const auto& list = kind == ElementKind::face ? looks.faces : looks.edges;
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const NamedLook& entry) { return entry.name == name; });
    REQUIRE(found != list.end());
    return *found;
}

// "The user clicked it": a reference made from what the body looks like now.
PersistentRef refTo(const BodyLooks& looks, ElementKind kind, const std::string& name) {
    const NamedLook& entry = named(looks, kind, name);
    return {kind, entry.name, entry.look};
}

const Placement kNoPlacement{};

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

TEST_CASE("resolve: size changes keep references to faces and edges by name", "[resolve]") {
    World before;
    const BodyLooks first = before.see(before.plate()).looks;
    const PersistentRef top = refTo(first, ElementKind::face, "F1/box/top");
    const PersistentRef edge = refTo(first, ElementKind::edge, "edge[F1/box/right|F1/box/top]");

    for (const auto& sizes : std::vector<std::array<const char*, 3>>{
             {"40", "40", "25"}, {"70", "40", "10"}, {"40", "15", "10"}, {"90", "30", "5"}}) {
        World after;
        const BodyLooks now = after.see(after.plate(sizes[0], sizes[1], sizes[2])).looks;
        for (const PersistentRef* ref : {&top, &edge}) {
            const ResolveOutcome outcome = resolve(*ref, now, {}, kNoPlacement);
            INFO(outcome.detail);
            REQUIRE(outcome.state == ResolveOutcome::State::found);
            CHECK(outcome.by == FoundBy::name);
            CHECK(outcome.element == ref->name);
            CHECK(outcome.score == Approx(1.0));
        }
    }
}

TEST_CASE("resolve: a replaced step is found again by similarity", "[resolve]") {
    World before;
    const BodyLooks first = before.see(before.plate()).looks;
    const PersistentRef top = refTo(first, ElementKind::face, "F1/box/top");
    const PersistentRef side = refTo(first, ElementKind::face, "F1/box/right");

    // The first box is deleted and an equal one made: new number, so new names.
    World after;
    const FeatureId dummy = after.plate("10", "10", "10");
    after.document.removeFeature(dummy);
    const BodyLooks now = after.see(after.plate()).looks;
    REQUIRE(now.faces.size() == 6);
    for (const PersistentRef* ref : {&top, &side}) {
        const ResolveOutcome outcome = resolve(*ref, now, {}, kNoPlacement);
        INFO(outcome.detail);
        REQUIRE(outcome.state == ResolveOutcome::State::found);
        CHECK(outcome.by == FoundBy::similarity);
        CHECK(outcome.score >= 0.80);
        CHECK(now.faces[outcome.index].look.direction.z ==
              Approx(ref->look.direction.z).margin(1e-9));
        CHECK(now.faces[outcome.index].look.direction.x ==
              Approx(ref->look.direction.x).margin(1e-9));
    }
}

TEST_CASE("resolve: a face cut in two needs help", "[resolve][evil]") {
    World before;
    const BodyLooks first = before.see(before.plate()).looks;
    const PersistentRef top = refTo(first, ElementKind::face, "F1/box/top");
    const PersistentRef rim = refTo(first, ElementKind::edge, "edge[F1/box/front|F1/box/top]");

    World after;
    const FeatureId plate = after.plate();
    const FeatureId slot = after.add(
        "box", {{"width", "10"}, {"depth", "60"}, {"height", "20"}, {"x", "0"}, {"z", "5"}});
    const FeatureId cut =
        after.add("combine", {}, {after.bodyOf(plate), after.bodyOf(slot)}, {{"mode", "subtract"}});
    const World::Seen now = after.see(cut);
    for (const PersistentRef* ref : {&top, &rim}) {
        const ResolveOutcome outcome = resolve(*ref, now.looks, now.consumed, kNoPlacement);
        INFO(outcome.detail);
        REQUIRE(outcome.state == ResolveOutcome::State::needsHelp);
        CHECK(outcome.reason == HelpReason::split);
    }
}

TEST_CASE("resolve: a face that a step covers needs help and is not mistaken for its look-alike",
          "[resolve][evil]") {
    World before;
    const BodyLooks first = before.see(before.plate()).looks;
    const PersistentRef top = refTo(first, ElementKind::face, "F1/box/top");

    // A box with the same footprint on top: the old top is inside, a new top is 10 mm higher.
    World after;
    const FeatureId plate = after.plate();
    const FeatureId boss = after.add(
        "box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}, {"z", "10"}});
    const FeatureId merged =
        after.add("combine", {}, {after.bodyOf(plate), after.bodyOf(boss)}, {{"mode", "merge"}});
    const World::Seen now = after.see(merged);
    const ResolveOutcome outcome = resolve(top, now.looks, now.consumed, kNoPlacement);
    INFO(outcome.detail);
    REQUIRE(outcome.state == ResolveOutcome::State::needsHelp);
    CHECK(outcome.reason == HelpReason::consumed);
}

TEST_CASE("resolve: moving or turning the whole body does not make references look changed",
          "[resolve][placement]") {
    const auto lookAt = [](const fission::geo::NamedSolid& body) {
        std::vector<BodyFace> faces;
        for (const fission::geo::NamedFace& face : body.faces) {
            faces.push_back({face.faceId, "F1/box/" + face.role});
        }
        const auto kernel = fission::geo::occt::makeKernel();
        return describeBody(faces, kernel->signatures(body.solid));
    };
    const fission::geo::NamedSolid box =
        fission::geo::occt::makeNamedBox(40.0, 20.0, 10.0, {-20.0, -10.0, 0.0});
    const BodyLooks first = lookAt(box);
    const PersistentRef right = refTo(first, ElementKind::face, "F1/box/right");
    const PersistentRef edge = refTo(first, ElementKind::edge, "edge[F1/box/right|F1/box/top]");

    const geo::Point3d centre{0.0, 0.0, 5.0};
    const geo::Angles angles{0.0, 0.0, 90.0};
    const geo::Point3d offset{30.0, -12.0, 0.0};
    const BodyLooks moved = lookAt(fission::geo::occt::translateNamed(
        fission::geo::occt::rotateNamed(box, centre, angles), offset));

    const Placement placement{centre, angles, offset};
    for (const PersistentRef* ref : {&right, &edge}) {
        const ResolveOutcome outcome = resolve(*ref, moved, {}, placement);
        INFO(outcome.detail);
        REQUIRE(outcome.state == ResolveOutcome::State::found);
        CHECK(outcome.element == ref->name);
    }
    // Without the placement the 90° turn of the right face would look like a change.
    const ResolveOutcome naive = resolve(right, moved, {}, kNoPlacement);
    CHECK(naive.state == ResolveOutcome::State::needsHelp);
    CHECK(naive.reason == HelpReason::turned);
}

TEST_CASE("resolve: two equal elements that swapped places need help", "[resolve][evil]") {
    // Two equal faces, "A" on the left and "B" on the right, then the names are exchanged.
    const auto bodyWith = [](const std::string& left, const std::string& right) {
        BodyLooks looks;
        looks.diagonal = 100.0;
        looks.faces.push_back({1,
                               left,
                               {ElementKind::face,
                                ElementShape::plane,
                                {-20, 0, 0},
                                {0, 0, 1},
                                true,
                                true,
                                100.0,
                                0.0,
                                {},
                                {}}});
        looks.faces.push_back({2,
                               right,
                               {ElementKind::face,
                                ElementShape::plane,
                                {20, 0, 0},
                                {0, 0, 1},
                                true,
                                true,
                                100.0,
                                0.0,
                                {},
                                {}}});
        return looks;
    };
    const BodyLooks before = bodyWith("A", "B");
    const PersistentRef ref = refTo(before, ElementKind::face, "A");
    const BodyLooks swapped = bodyWith("B", "A");

    const ResolveOutcome outcome = resolve(ref, swapped, {}, kNoPlacement);
    REQUIRE(outcome.state == ResolveOutcome::State::needsHelp);
    CHECK(outcome.reason == HelpReason::swapped);

    // The check is needed: without it the reference silently lands on the wrong face.
    ResolveSwitches off;
    off.checkSwap = false;
    const ResolveOutcome wrong = resolve(ref, swapped, {}, kNoPlacement, {}, off);
    REQUIRE(wrong.state == ResolveOutcome::State::found);
    CHECK(wrong.id == 2);

    // Unchanged names are fine.
    CHECK(resolve(ref, before, {}, kNoPlacement).state == ResolveOutcome::State::found);
}

TEST_CASE("resolve: a name that turned by more than 45 degrees needs help", "[resolve][evil]") {
    BodyLooks before;
    before.diagonal = 100.0;
    before.faces.push_back({1, "S", flatFace(0, 0, 0, {1, 0, 0})});
    const PersistentRef ref = refTo(before, ElementKind::face, "S");

    const auto turnedBy = [&](double degrees) {
        BodyLooks now;
        now.diagonal = 100.0;
        const double radians = degrees * std::numbers::pi / 180.0;
        now.faces.push_back(
            {1, "S", flatFace(0, 0, 0, {std::cos(radians), std::sin(radians), 0.0})});
        return resolve(ref, now, {}, kNoPlacement);
    };
    CHECK(turnedBy(0.0).state == ResolveOutcome::State::found);
    CHECK(turnedBy(40.0).state == ResolveOutcome::State::found);
    const ResolveOutcome turned = turnedBy(50.0);
    CHECK(turned.state == ResolveOutcome::State::needsHelp);
    CHECK(turned.reason == HelpReason::turned);
    CHECK(turnedBy(180.0).reason == HelpReason::turned);
}

TEST_CASE("resolve: a name that now points to another kind of surface needs help",
          "[resolve][evil]") {
    BodyLooks before;
    before.diagonal = 100.0;
    before.faces.push_back({1, "S", flatFace(0, 0, 0, {0, 0, 1})});
    const PersistentRef ref = refTo(before, ElementKind::face, "S");
    BodyLooks now = before;
    now.faces[0].look.shape = ElementShape::cylinder;
    CHECK(resolve(ref, now, {}, kNoPlacement).state == ResolveOutcome::State::needsHelp);
}

TEST_CASE("resolve: refreshing takes the current name and look, in the step's own coordinates",
          "[resolve]") {
    World before;
    const BodyLooks first = before.see(before.plate()).looks;
    const PersistentRef top = refTo(first, ElementKind::face, "F1/box/top");

    World after;
    const BodyLooks now = after.see(after.plate("40", "40", "30")).looks;
    const Placement placement{{}, {}, {5.0, 0.0, 0.0}};
    BodyLooks inWorld = now;
    for (NamedLook& face : inWorld.faces) {
        face.look.centroid.x += 5.0; // the body sits 5 mm further along X
    }
    const ResolveOutcome outcome = resolve(top, inWorld, {}, placement);
    REQUIRE(outcome.state == ResolveOutcome::State::found);
    const PersistentRef healed = refreshed(top, inWorld, outcome, placement);
    CHECK(healed.name == top.name);
    CHECK(healed.look.centroid.z == Approx(30.0));
    CHECK(healed.look.centroid.x == Approx(0.0).margin(1e-9));
    CHECK(healed.look.size == Approx(1600.0));

    // A reference that needs help stays as it was.
    BodyLooks empty;
    empty.faces.push_back({1, "other", flatFace(500, 0, 0, {1, 0, 0})});
    empty.diagonal = 10.0;
    const ResolveOutcome lost = resolve(top, empty, {}, kNoPlacement);
    REQUIRE(lost.state == ResolveOutcome::State::needsHelp);
    CHECK(refreshed(top, empty, lost, kNoPlacement) == top);
}

TEST_CASE("resolve: nothing to resolve in an empty body", "[resolve]") {
    PersistentRef ref;
    ref.name = "F1/box/top";
    const ResolveOutcome outcome = resolve(ref, BodyLooks{}, {}, kNoPlacement);
    CHECK(outcome.state == ResolveOutcome::State::needsHelp);
    CHECK(outcome.reason == HelpReason::notFound);
}
