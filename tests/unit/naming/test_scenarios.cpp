// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The scenario suite of persistent references with the product code (AP-2.10d): every change a
// person can make today to a model that holds a reference, judged against the geometry. Silent
// mistakes must be 0 (rule 6), and at least 90 % of the references with a right answer must be
// found without help. The scenarios with fillets, holes and sketches are added with the tools
// that make them (AP-2.5, AP-2.7, AP-2.3). Never weaken these tests.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "judge.h"

using namespace fission::core;
using namespace fission::test;
namespace geo = fission::geo;

namespace {

const geo::Point3d kTop{0, 0, 1};
const geo::Point3d kBottom{0, 0, -1};
const geo::Point3d kLeft{-1, 0, 0};
const geo::Point3d kRight{1, 0, 0};
const geo::Point3d kFront{0, -1, 0};
const geo::Point3d kBack{0, 1, 0};

struct Side {
    const char* role;
    geo::Point3d normal;
};
const std::vector<Side> kSides{{"top", kTop},     {"bottom", kBottom}, {"left", kLeft},
                               {"right", kRight}, {"front", kFront},   {"back", kBack}};

const Side& sideNamed(const std::string& role) {
    return *std::find_if(kSides.begin(), kSides.end(),
                         [&](const Side& s) { return role == s.role; });
}

std::string faceName(const std::string& role, const char* feature = "F1/box/") {
    return std::string(feature) + role;
}

std::string edgeName(std::string a, std::string b) {
    a = faceName(a);
    b = faceName(b);
    if (b < a) {
        std::swap(a, b);
    }
    return "edge[" + a + "|" + b + "]";
}

enum class Expect {
    found, ///< Must be found without help.
    help,  ///< Must ask for help (there is no single right element).
    any    ///< Not wrong; found or help are both acceptable.
};

/// A model with one step that holds the reference (`mark`) and the step whose body it points
/// into (`target`).
struct Fixture {
    RefWorld world;
    FeatureId plate;  ///< The box.
    FeatureId target; ///< The step the reference points into (the box, or a combine on it).
    FeatureId mark;
    FeatureId extra; ///< A tool or an unrelated step some scenarios change.
    ElementKind kind = ElementKind::face;
};

Fixture plateFixture(ElementKind kind, const std::string& name) {
    Fixture f;
    f.kind = kind;
    f.plate = f.world.plate("40", "30", "10");
    f.target = f.plate;
    const RecomputeResult first = f.world.run();
    f.mark = f.world.mark_(f.target, f.world.pick(first, f.target, kind, name));
    return f;
}

/// A box with a second box that is first out of reach and joined to it by the change.
Fixture combineFixture(const char* mode, ElementKind kind, const std::string& name,
                       const char* toolHeight, const char* toolWidth, const char* toolX,
                       const char* toolZ) {
    Fixture f;
    f.kind = kind;
    f.plate = f.world.plate("40", "30", "10");
    f.extra = f.world.add("box", {{"width", toolWidth},
                                  {"depth", "60"},
                                  {"height", toolHeight},
                                  {"x", toolX},
                                  {"z", toolZ}});
    f.target = f.world.add("combine", {}, {f.world.bodyOf(f.plate), f.world.bodyOf(f.extra)},
                           {{"mode", mode}});
    const RecomputeResult first = f.world.run();
    f.mark = f.world.mark_(f.target, f.world.pick(first, f.target, kind, name));
    return f;
}

struct Totals {
    Tally all;
    std::vector<std::string> wrong;
};

Totals& totals() {
    static Totals instance;
    return instance;
}

void judged(const std::string& name, Fixture& f,
            const std::function<Truth(const BodyLooks&)>& truth, Expect expect) {
    const RecomputeResult result = f.world.run();
    applyHealedRefs(f.world.document, result);
    const BodyLooks looks = f.world.looksOf(result, f.target);
    const Verdict verdict = judge(f.world, result, f.mark, truth(looks));
    totals().all.add(verdict);
    INFO(name);
    CHECK(verdict != Verdict::silentlyWrong);
    if (verdict == Verdict::silentlyWrong) {
        totals().wrong.push_back(name);
    }
    switch (expect) {
    case Expect::found:
        CHECK(verdict == Verdict::correct);
        break;
    case Expect::help:
        CHECK(verdict == Verdict::helpPlus);
        break;
    case Expect::any:
        break;
    }
}

Truth faceTruth(const BodyLooks& looks, const std::string& role, double turn = 0.0) {
    return facesLooking(looks, turnedAboutZ(sideNamed(role).normal, turn));
}

Truth edgeTruth(const BodyLooks& looks, const std::string& a, const std::string& b,
                double turn = 0.0) {
    return edgesBetween(looks, turnedAboutZ(sideNamed(a).normal, turn),
                        turnedAboutZ(sideNamed(b).normal, turn));
}

// Reports what the test case judged, when it ends (`scripts/test` adds the lines up, and fails
// when a reference was silently wrong or fewer than 90 % were found by themselves).
struct Flush {
    Flush() : before(totals().all) {}
    ~Flush() {
        Tally delta;
        delta.correct = totals().all.correct - before.correct;
        delta.helpPlus = totals().all.helpPlus - before.helpPlus;
        delta.helpMinus = totals().all.helpMinus - before.helpMinus;
        delta.wrong = totals().all.wrong - before.wrong;
        report("scenarios", delta);
    }
    Tally before;
};

std::size_t position(const Document& document, const FeatureId& id) {
    const auto& features = document.features();
    for (std::size_t i = 0; i < features.size(); ++i) {
        if (features[i].id == id) {
            return i;
        }
    }
    return features.size();
}

// The person replaces the box by an equal new one (new number, new names) and points the step
// at the new box.
void replacePlate(Fixture& f) {
    FeatureDraft draft = defaultDraft(*f.world.registry.find("box"));
    draft.params.at("width").expression = "40";
    draft.params.at("depth").expression = "30";
    draft.params.at("height").expression = "10";
    draft.params.at("x").expression = "0";
    const FeatureId fresh =
        *f.world.document.addFeature(std::move(draft), position(f.world.document, f.mark));
    REQUIRE(f.world.document.setFeatureInputs(f.mark, {f.world.bodyOf(fresh)}));
    REQUIRE(f.world.document.removeFeature(f.plate));
    f.plate = fresh;
    f.target = fresh;
}

} // namespace

TEST_CASE("naming-product: sizes change, faces keep their names", "[naming-product]") {
    const Flush flush;
    struct Change {
        const char* what;
        std::vector<std::pair<const char*, const char*>> values;
    };
    const std::vector<Change> changes{
        {"height", {{"height", "25"}}},
        {"width", {{"width", "70"}}},
        {"depth", {{"depth", "12"}}},
        {"all three", {{"width", "55"}, {"depth", "22"}, {"height", "6"}}}};
    for (const Side& side : kSides) {
        for (const Change& change : changes) {
            Fixture f = plateFixture(ElementKind::face, faceName(side.role));
            for (const auto& [name, value] : change.values) {
                REQUIRE(f.world.document.setFeatureValue(f.plate, name, value));
            }
            judged(
                std::string("size: ") + side.role + " / " + change.what, f,
                [&](const BodyLooks& looks) { return faceTruth(looks, side.role); }, Expect::found);
        }
    }
}

TEST_CASE("naming-product: sizes change, edges keep their names", "[naming-product]") {
    const Flush flush;
    const std::vector<std::pair<const char*, const char*>> edges{
        {"top", "right"}, {"top", "front"}, {"bottom", "left"}, {"front", "right"}};
    const std::vector<std::pair<const char*, const char*>> changes{
        {"height", "25"}, {"width", "70"}, {"depth", "12"}, {"height", "3"}};
    for (const auto& [a, b] : edges) {
        for (const auto& [what, value] : changes) {
            Fixture f = plateFixture(ElementKind::edge, edgeName(a, b));
            REQUIRE(f.world.document.setFeatureValue(f.plate, what, value));
            judged(
                std::string("size: edge ") + a + "/" + b + " / " + what, f,
                [&](const BodyLooks& looks) { return edgeTruth(looks, a, b); }, Expect::found);
        }
    }
}

TEST_CASE("naming-product: steps before and beside the box change nothing", "[naming-product]") {
    const Flush flush;
    for (const char* role : {"top", "right"}) {
        Fixture f = plateFixture(ElementKind::face, faceName(role));
        FeatureDraft draft = defaultDraft(*f.world.registry.find("cylinder"));
        REQUIRE(f.world.document.addFeature(std::move(draft), 0).has_value());
        judged(
            std::string("insert before: ") + role, f,
            [&](const BodyLooks& looks) { return faceTruth(looks, role); }, Expect::found);
    }
    Fixture edge = plateFixture(ElementKind::edge, edgeName("top", "front"));
    FeatureDraft draft = defaultDraft(*edge.world.registry.find("cylinder"));
    REQUIRE(edge.world.document.addFeature(std::move(draft), 0).has_value());
    judged(
        "insert before: edge", edge,
        [&](const BodyLooks& looks) { return edgeTruth(looks, "top", "front"); }, Expect::found);

    Fixture f = plateFixture(ElementKind::face, faceName("top"));
    const FeatureId other = f.world.add("cylinder");
    f.world.run();
    REQUIRE(f.world.document.setFeatureSuppressed(other, true));
    judged(
        "switch an unrelated step off", f,
        [&](const BodyLooks& looks) { return faceTruth(looks, "top"); }, Expect::found);
}

TEST_CASE("naming-product: moving and turning the box", "[naming-product]") {
    const Flush flush;
    for (const char* role : {"top", "right"}) {
        Fixture f = plateFixture(ElementKind::face, faceName(role));
        REQUIRE(f.world.document.setFeatureValue(f.plate, "x", "50"));
        judged(
            std::string("move: ") + role, f,
            [&](const BodyLooks& looks) { return faceTruth(looks, role); }, Expect::found);
    }
    for (const char* role : {"right", "front"}) {
        for (const double turn : {90.0, 180.0, 37.0}) {
            Fixture f = plateFixture(ElementKind::face, faceName(role));
            REQUIRE(f.world.document.setFeatureValue(f.plate, "rotZ", std::to_string(turn)));
            judged(
                std::string("turn: ") + role + " by " + std::to_string(int(turn)), f,
                [&](const BodyLooks& looks) { return faceTruth(looks, role, turn); },
                Expect::found);
        }
    }
}

TEST_CASE("naming-product: a box that was replaced by an equal one", "[naming-product]") {
    const Flush flush;
    for (const Side& side : kSides) {
        Fixture f = plateFixture(ElementKind::face, faceName(side.role));
        replacePlate(f);
        judged(
            std::string("replaced: ") + side.role, f,
            [&](const BodyLooks& looks) { return faceTruth(looks, side.role); }, Expect::any);
    }
}

TEST_CASE("naming-product: cylinders", "[naming-product]") {
    const Flush flush;
    struct Change {
        const char* what;
        std::vector<std::pair<const char*, const char*>> values;
    };
    const std::vector<Change> changes{{"radius", {{"radius", "14"}}},
                                      {"height", {{"height", "40"}}},
                                      {"both", {{"radius", "6"}, {"height", "8"}}}};
    for (const char* role : {"side", "top"}) {
        for (const Change& change : changes) {
            Fixture f;
            f.plate = f.world.add("cylinder", {{"radius", "10"}, {"height", "20"}});
            f.target = f.plate;
            const RecomputeResult first = f.world.run();
            f.mark = f.world.mark_(f.target, f.world.pick(first, f.target, ElementKind::face,
                                                          faceName(role, "F1/cylinder/")));
            for (const auto& [name, value] : change.values) {
                REQUIRE(f.world.document.setFeatureValue(f.plate, name, value));
            }
            judged(
                std::string("cylinder: ") + role + " / " + change.what, f,
                [&](const BodyLooks& looks) {
                    return std::string(role) == "side" ? facesOfShape(looks, ElementShape::cylinder)
                                                       : facesLooking(looks, kTop);
                },
                Expect::found);
        }
    }
}

TEST_CASE("naming-product: a slot that cuts the top in two needs help", "[naming-product][evil]") {
    const Flush flush;
    // The slot is first too shallow to reach the top; then it grows through it.
    const auto make = [](ElementKind kind, const std::string& name) {
        return combineFixture("subtract", kind, name, "4", "10", "0", "5");
    };
    Fixture top = make(ElementKind::face, faceName("top"));
    REQUIRE(top.world.document.setFeatureValue(top.extra, "height", "20"));
    judged(
        "slot: top is cut in two", top,
        [&](const BodyLooks& looks) { return facesLookingAt(looks, kTop, 10.0); }, Expect::help);

    Fixture edge = make(ElementKind::edge, edgeName("top", "front"));
    REQUIRE(edge.world.document.setFeatureValue(edge.extra, "height", "20"));
    judged(
        "slot: top edge is cut in two", edge,
        [&](const BodyLooks& looks) { return edgeTruth(looks, "top", "front"); }, Expect::help);

    // The bottom is not touched by the slot: found.
    Fixture bottom = make(ElementKind::face, faceName("bottom"));
    REQUIRE(bottom.world.document.setFeatureValue(bottom.extra, "height", "20"));
    judged(
        "slot: the bottom stays", bottom,
        [&](const BodyLooks& looks) { return facesLooking(looks, kBottom); }, Expect::found);
}

TEST_CASE("naming-product: a face that a step covers needs help", "[naming-product][evil]") {
    const Flush flush;
    // A second box with the same footprint is first far away, then put on the top.
    const auto stacked = [](const char* height) {
        Fixture f =
            combineFixture("merge", ElementKind::face, faceName("top"), height, "40", "0", "100");
        REQUIRE(f.world.document.setFeatureValue(f.extra, "depth", "30"));
        REQUIRE(f.world.document.setFeatureValue(f.extra, "z", "10"));
        return f;
    };
    Fixture tall = stacked("10");
    judged(
        "covered: by a tall box", tall,
        [&](const BodyLooks& looks) { return facesLookingAt(looks, kTop, 10.0); }, Expect::help);

    // The new top is only half a millimetre higher: it looks almost like the old one, and must
    // not be taken for it.
    Fixture thin = stacked("0.5");
    judged(
        "covered: by a thin box", thin,
        [&](const BodyLooks& looks) { return facesLookingAt(looks, kTop, 10.0); }, Expect::help);
}

TEST_CASE("naming-product: a face that was joined into another", "[naming-product]") {
    const Flush flush;
    // The top of a second box next to the first one lies in the same plane and becomes part of
    // the top of the first.
    Fixture f = combineFixture("merge", ElementKind::face, faceName("top", "F2/box/"), "10", "40",
                               "100", "0");
    REQUIRE(f.world.document.setFeatureValue(f.extra, "depth", "30"));
    REQUIRE(f.world.document.setFeatureValue(f.extra, "x", "30"));
    judged(
        "joined: the top of the second box", f,
        [&](const BodyLooks& looks) { return facesLookingAt(looks, kTop, 10.0); }, Expect::any);
}

namespace {

// The right wall of the box is pulled out (or the box is stretched there) by a step of its own; the
// reference points into the body that step made (AP-2.4).
Fixture pulledFixture(const char* type, const std::string& markRole) {
    Fixture f;
    f.plate = f.world.plate("40", "30", "10");
    const RecomputeResult first = f.world.run();
    FeatureDraft draft = defaultDraft(*f.world.registry.find(type));
    draft.inputs = {f.world.bodyOf(f.plate)};
    draft.refs["face"] = {f.world.pick(first, f.plate, ElementKind::face, faceName("right"))};
    draft.params.at("distance").expression = "5";
    if (std::string(type) == "stretch") {
        draft.params.at("position").expression = "0"; // the middle of the box: nothing there
    }
    f.extra = *f.world.document.addFeature(std::move(draft));
    f.target = f.extra;
    const RecomputeResult second = f.world.run();
    f.mark = f.world.mark_(f.target,
                           f.world.pick(second, f.target, ElementKind::face, faceName(markRole)));
    return f;
}

} // namespace

TEST_CASE("naming-product: pulling and stretching faces keep every name", "[naming-product]") {
    const Flush flush;
    for (const char* type : {"pushFace", "stretch"}) {
        for (const char* role : {"top", "right", "left", "front"}) {
            const std::string label = std::string(type) + ": " + role;
            Fixture longer = pulledFixture(type, role);
            REQUIRE(longer.world.document.setFeatureValue(longer.extra, "distance", "20"));
            judged(
                label + " after pulling further", longer,
                [&](const BodyLooks& looks) { return faceTruth(looks, role); }, Expect::found);

            Fixture wider = pulledFixture(type, role);
            REQUIRE(wider.world.document.setFeatureValue(wider.plate, "width", "60"));
            judged(
                label + " after the box got wider", wider,
                [&](const BodyLooks& looks) { return faceTruth(looks, role); }, Expect::found);
        }
    }
}
