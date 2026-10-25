// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/history.h"

using namespace fission::core;

namespace {

FeatureDraft boxDraft(std::string width = "20") {
    FeatureDraft draft;
    draft.type = "box";
    draft.params["width"] = {std::move(width), QuantityKind::length};
    draft.params["depth"] = {"10", QuantityKind::length};
    draft.newBodies = 1;
    return draft;
}

CommandDescription group() {
    return {"group", {}};
}

// A random but reproducible sequence of changes of every kind the commands know.
class RandomEditor {
public:
    explicit RandomEditor(std::uint64_t seed) : random_(seed) {}

    // Builds a command that makes sense for the current document, or one that is refused.
    std::unique_ptr<Command> next(const Document& document) {
        const auto& features = document.features();
        const auto& bodies = document.bodies();
        const auto& parameters = document.parameters();
        switch (pick(15)) {
        case 0:
        case 1: { // add (sometimes in the middle, sometimes working on an earlier body)
            FeatureDraft draft = boxDraft(std::to_string(pick(50)));
            std::optional<std::size_t> position;
            if (!features.empty() && pick(3) == 0) {
                position = pick(features.size() + 1);
            }
            if (!bodies.empty() && pick(2) == 0) {
                const std::size_t at = position.value_or(features.size());
                for (const Body& body : bodies) {
                    const auto maker = document.indexOf(body.createdBy);
                    if (maker && *maker < at) {
                        draft.inputs.push_back(body.id);
                        break;
                    }
                }
            }
            return std::make_unique<AddFeatureCommand>(std::move(draft), position);
        }
        case 2:
            if (!features.empty()) {
                return makeRemoveFeature(features[pick(features.size())].id);
            }
            break;
        case 3:
        case 4:
            if (!features.empty()) {
                static const char* const values[] = {"1", "2,5", "3 cm", "Breite + 1", "7", "-4"};
                static const char* const names[] = {"width", "depth", "nonsense"};
                return makeSetFeatureValue(features[pick(features.size())].id, names[pick(3)],
                                           values[pick(6)]);
            }
            break;
        case 5:
            if (!features.empty()) {
                return makeSetFeatureLabel(features[pick(features.size())].id,
                                           pick(2) == 0 ? "" : "Name " + std::to_string(pick(5)));
            }
            break;
        case 6:
            if (!features.empty()) {
                return makeSetFeatureSuppressed(features[pick(features.size())].id, pick(2) == 0);
            }
            break;
        case 7:
            if (!bodies.empty()) {
                return makeSetBodyName(bodies[pick(bodies.size())].id,
                                       "Teil " + std::to_string(pick(4)));
            }
            break;
        case 8:
            if (!bodies.empty()) {
                return makeSetBodyColor(bodies[pick(bodies.size())].id,
                                        Color{static_cast<std::uint8_t>(pick(256)), 20,
                                              static_cast<std::uint8_t>(pick(256))});
            }
            break;
        case 9:
            if (!bodies.empty()) {
                return makeSetBodyVisible(bodies[pick(bodies.size())].id, pick(2) == 0);
            }
            break;
        case 10: // names repeat on purpose: mistakes are not refused (document.h)
            return makeAddParameter({"M" + std::to_string(pick(3)), std::to_string(pick(100)),
                                     QuantityKind::length, ""});
        case 11:
            if (!parameters.empty()) {
                const std::string& name = parameters[pick(parameters.size())].name;
                return pick(2) == 0 ? makeRemoveParameter(name)
                                    : makeSetParameterExpression(name, std::to_string(pick(100)));
            }
            break;
        case 12:
            if (!parameters.empty()) {
                return makeRenameParameter(parameters[pick(parameters.size())].name,
                                           "M" + std::to_string(pick(3)));
            }
            break;
        default: // the time marker anywhere, or back at the end
            return makeSetRollback(pick(3) == 0
                                       ? std::nullopt
                                       : std::optional<std::size_t>(pick(features.size() + 1)));
        }
        return makeSetFeatureValue(FeatureId{"not-there"}, "width", "1"); // refused
    }

    std::size_t pick(std::size_t count) {
        return std::uniform_int_distribution<std::size_t>(0, count - 1)(random_);
    }

private:
    std::mt19937_64 random_;
};

} // namespace

TEST_CASE("undoing 1000 random commands gives the starting document again", "[history]") {
    for (const std::uint64_t seed : {1u, 2u, 3u, 4u, 5u}) {
        History history{Document(seed)};
        history.addFeature(boxDraft());
        history.addFeature(boxDraft("35"));
        history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
        const Document start = history.document();
        const std::size_t startPosition = history.position();

        RandomEditor editor(seed * 7919);
        for (int i = 0; i < 1000; ++i) {
            history.execute(editor.next(history.document()));
        }
        const Document end = history.document();
        REQUIRE(history.position() > startPosition);

        while (history.position() > startPosition) {
            REQUIRE(history.undo());
        }
        CHECK(history.document().sameContentAs(start));

        // And forward again: exactly the document we had, same ids and numbers included.
        while (history.canRedo()) {
            REQUIRE(history.redo());
        }
        CHECK(history.document().sameContentAs(end));
    }
}

TEST_CASE("every state on the way is exactly the state the document had then", "[history]") {
    // A plain copy of the document after each step is the reference: undo, redo and jumping
    // must always land on that copy, with random new changes cutting off the redo line.
    for (const std::uint64_t seed : {11u, 12u, 13u}) {
        History history{Document(seed)};
        std::vector<Document> states{history.document()}; // states[k] = after k steps
        RandomEditor editor(seed * 104729);

        for (int i = 0; i < 600; ++i) {
            switch (editor.pick(8)) {
            case 0:
                history.undo();
                break;
            case 1:
                history.redo();
                break;
            case 2:
                history.goTo(editor.pick(history.steps().size() + 1));
                break;
            default: {
                const std::size_t before = history.position();
                const std::size_t total = history.steps().size();
                REQUIRE_NOTHROW(history.execute(editor.next(history.document())));
                if (history.position() > before) { // a step was recorded
                    states.resize(before + 1);
                    states.push_back(history.document());
                    CHECK(history.position() == before + 1);
                    CHECK(history.steps().size() == before + 1);
                } else {
                    CHECK(history.steps().size() == total);
                }
                break;
            }
            }
            REQUIRE(history.position() < states.size());
            REQUIRE(history.document().sameContentAs(states[history.position()]));
        }
    }
}

TEST_CASE("a drag makes exactly one undo step", "[history]") {
    History history{Document(1)};
    const auto id = history.addFeature(boxDraft("20"));
    REQUIRE(id);
    const std::size_t before = history.position();

    history.beginTransaction({"setValue", {{"field", "width"}}});
    for (int i = 1; i <= 200; ++i) {
        REQUIRE(history.execute(makeSetFeatureValue(*id, "width", std::to_string(20 + i))));
        // the document follows the mouse right away
        CHECK(history.document().findFeature(*id)->params.at("width").expression ==
              std::to_string(20 + i));
    }
    CHECK(history.inTransaction());
    CHECK_FALSE(history.canUndo()); // nothing to undo until the drag is over
    history.commitTransaction();

    CHECK(history.position() == before + 1);
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "220");
    CHECK(history.nextUndo()->key == "setValue");

    REQUIRE(history.undo());
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "20");
    REQUIRE(history.redo());
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "220");
}

TEST_CASE("a step with a name of its own keeps it, even for a single change", "[history]") {
    History history{Document(7)};
    const auto id = history.addFeature(boxDraft("20"));
    history.beginTransaction({"moveBody", {{"type", "box"}}});
    REQUIRE(history.execute(makeSetFeatureValue(*id, "width", "5")));
    REQUIRE(history.execute(makeSetFeatureValue(*id, "width", "12")));
    history.commitTransaction();
    REQUIRE(history.nextUndo());
    CHECK(history.nextUndo()->key == "moveBody");
    REQUIRE(history.undo());
    REQUIRE(history.document().findFeature(*id) != nullptr);
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "20");
}

TEST_CASE("a value learnt during the drag goes into the name of the step", "[history]") {
    History history{Document(7)};
    const auto id = history.addFeature(boxDraft("20"));
    history.setTransactionValue("value", "999"); // no transaction: nothing happens
    history.beginTransaction({"scaleBody", {{"type", "box"}}});
    REQUIRE(history.execute(makeSetFeatureValue(*id, "width", "30")));
    history.setTransactionValue("value", "150");
    REQUIRE(history.execute(makeSetFeatureValue(*id, "width", "40")));
    history.setTransactionValue("value", "200");
    history.commitTransaction();
    REQUIRE(history.nextUndo());
    CHECK(history.nextUndo()->key == "scaleBody");
    CHECK(history.nextUndo()->values.at("value") == "200");
    CHECK(history.nextUndo()->values.at("type") == "box");
}

TEST_CASE("a drag of several values still is one step and undoes in the right order", "[history]") {
    History history{Document(2)};
    const auto id = history.addFeature(boxDraft("20"));
    const Document start = history.document();

    history.beginTransaction(group());
    for (int i = 1; i <= 50; ++i) {
        history.execute(makeSetFeatureValue(*id, "width", std::to_string(i)));
        history.execute(makeSetFeatureValue(*id, "depth", std::to_string(100 + i)));
    }
    history.commitTransaction();
    CHECK(history.position() == 2);

    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(start));
}

TEST_CASE("a drag that ends where it started leaves no step", "[history]") {
    History history{Document(3)};
    const auto id = history.addFeature(boxDraft("20"));
    const std::size_t before = history.position();

    history.beginTransaction(group());
    history.execute(makeSetFeatureValue(*id, "width", "35"));
    history.execute(makeSetFeatureValue(*id, "width", "20"));
    history.commitTransaction();
    CHECK(history.position() == before);

    history.beginTransaction(group());
    history.commitTransaction(); // nothing happened at all
    CHECK(history.position() == before);
}

TEST_CASE("a cancelled drag takes back what it did and leaves no step", "[history]") {
    History history{Document(4)};
    const auto id = history.addFeature(boxDraft("20"));
    const Document start = history.document();

    history.beginTransaction(group());
    history.execute(makeSetFeatureValue(*id, "width", "99"));
    history.execute(makeSetFeatureSuppressed(*id, true));
    history.cancelTransaction();

    CHECK_FALSE(history.inTransaction());
    CHECK(history.document().sameContentAs(start));
    CHECK(history.position() == 1);
}

TEST_CASE("nested transactions count once, at the outermost end", "[history]") {
    History history{Document(5)};
    const auto id = history.addFeature(boxDraft());
    history.beginTransaction(group());
    history.beginTransaction(group());
    history.execute(makeSetFeatureValue(*id, "width", "5"));
    history.commitTransaction();
    CHECK(history.inTransaction());
    history.execute(makeSetFeatureValue(*id, "depth", "6"));
    history.commitTransaction();
    CHECK_FALSE(history.inTransaction());
    CHECK(history.position() == 2);
}

TEST_CASE("a value that does not change and a refused command leave no step", "[history]") {
    History history{Document(6)};
    const auto id = history.addFeature(boxDraft("20"));
    const std::size_t before = history.position();

    CHECK(history.execute(makeSetFeatureValue(*id, "width", "20"))); // already 20
    CHECK(history.position() == before);

    CHECK_FALSE(history.execute(makeSetFeatureValue(FeatureId{"nope"}, "width", "1")));
    CHECK_FALSE(history.execute(makeSetFeatureValue(*id, "nonsense", "1")));
    CHECK_FALSE(history.execute(makeRemoveFeature(FeatureId{"nope"})));
    CHECK_FALSE(history.execute(makeSetBodyVisible(BodyId{"nope"}, false)));
    CHECK_FALSE(history.execute(makeRemoveParameter("nothing")));
    CHECK(history.position() == before);
}

TEST_CASE("a new change ends what could be redone", "[history]") {
    History history{Document(7)};
    const auto id = history.addFeature(boxDraft("20"));
    history.execute(makeSetFeatureValue(*id, "width", "30"));
    history.execute(makeSetFeatureValue(*id, "width", "40"));
    REQUIRE(history.undo());
    REQUIRE(history.undo());
    CHECK(history.canRedo());

    history.execute(makeSetFeatureValue(*id, "width", "50"));
    CHECK_FALSE(history.canRedo());
    CHECK(history.steps().size() == 2);
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "50");
}

TEST_CASE("removing a step and undoing it brings back the same step with its bodies", "[history]") {
    History history{Document(8)};
    const auto first = history.addFeature(boxDraft("10"));
    const auto second = history.addFeature(boxDraft("20"));
    const auto third = history.addFeature(boxDraft("30"));
    const Document before = history.document();
    const std::string bodyOfSecond = history.document().findFeature(*second)->outputs.at(0).value;

    REQUIRE(history.execute(makeRemoveFeature(*second)));
    CHECK(history.document().features().size() == 2);
    CHECK(history.document().bodies().size() == 2);

    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
    CHECK(history.document().findFeature(*second)->number == 2);
    CHECK(history.document().findFeature(*second)->outputs.at(0).value == bodyOfSecond);
    CHECK(history.document().indexOf(*second) == 1);
    CHECK(history.document().findFeature(*third) != nullptr);
    CHECK(history.document().bodies()[1].id.value == bodyOfSecond);
}

TEST_CASE("undoing a new step also takes back its number", "[history]") {
    History history{Document(9)};
    history.addFeature(boxDraft());
    const auto added = history.addFeature(boxDraft());
    CHECK(history.document().findFeature(*added)->number == 2);

    REQUIRE(history.undo());
    CHECK(history.document().features().size() == 1);
    CHECK(history.document().lastFeatureNumber() == 1);

    REQUIRE(history.redo()); // the same step comes back: same id, same number
    CHECK(history.document().findFeature(*added)->number == 2);
    CHECK(history.document().lastFeatureNumber() == 2);
}

TEST_CASE("steps tell what they were, as keys and values", "[history]") {
    History history{Document(10)};
    const auto id = history.addFeature(boxDraft("20"));
    history.execute(makeSetFeatureValue(*id, "width", "35"));
    history.execute(makeSetFeatureSuppressed(*id, true));
    const BodyId body = history.document().bodies()[0].id;
    history.execute(makeSetBodyVisible(body, false));
    history.execute(makeSetBodyName(body, "Fuß"));

    const auto steps = history.steps();
    REQUIRE(steps.size() == 5);
    CHECK(steps[0] == CommandDescription{"addFeature", {{"type", "box"}, {"number", "1"}}});
    CHECK(steps[1].key == "setValue");
    CHECK(steps[1].values.at("field") == "width");
    CHECK(steps[1].values.at("value") == "35");
    CHECK(steps[1].values.at("type") == "box");
    CHECK(steps[2].key == "suppress");
    CHECK(steps[3].key == "hideBody");
    CHECK(steps[4].key == "nameBody");
    CHECK(steps[4].values.at("value") == "Fuß");
    CHECK(history.nextUndo()->key == "nameBody");
    CHECK_FALSE(history.nextRedo());
}

TEST_CASE("going to a step in the list undoes or redoes as many steps as needed", "[history]") {
    History history{Document(11)};
    const auto id = history.addFeature(boxDraft("20"));
    for (int i = 1; i <= 5; ++i) {
        history.execute(makeSetFeatureValue(*id, "width", std::to_string(20 + i)));
    }
    REQUIRE(history.goTo(2));
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "21");
    CHECK(history.canRedo());
    REQUIRE(history.goTo(6));
    CHECK(history.document().findFeature(*id)->params.at("width").expression == "25");
    REQUIRE(history.goTo(0));
    CHECK(history.document().features().empty());
    CHECK_FALSE(history.goTo(99));
}

TEST_CASE("the saved state is remembered across undo and redo", "[history]") {
    History history{Document(12)};
    const auto id = history.addFeature(boxDraft("20"));
    CHECK_FALSE(history.isClean());
    history.markClean();
    CHECK(history.isClean());

    history.execute(makeSetFeatureValue(*id, "width", "30"));
    CHECK_FALSE(history.isClean());
    history.undo();
    CHECK(history.isClean());
    history.redo();
    CHECK_FALSE(history.isClean());

    // The saved state falls out of the line when a new change cuts it off.
    history.markClean();
    history.undo();
    history.execute(makeSetFeatureValue(*id, "width", "77"));
    CHECK_FALSE(history.isClean());
    history.undo();
    CHECK_FALSE(history.isClean()); // the saved state is gone for good
}

TEST_CASE("named measures: add, change, rename and remove can all be undone", "[history]") {
    History history{Document(13)};
    history.execute(makeAddParameter({"A", "10", QuantityKind::length, ""}));
    history.execute(makeAddParameter({"B", "A + 1", QuantityKind::length, "kommentar"}));
    const Document start = history.document();

    history.execute(makeSetParameterExpression("A", "12"));
    history.execute(makeRenameParameter("B", "C"));
    history.execute(makeRemoveParameter("A"));
    REQUIRE(history.document().parameters().size() == 1);
    CHECK(history.document().parameters()[0].name == "C");

    while (history.position() > 2) {
        REQUIRE(history.undo());
    }
    CHECK(history.document().sameContentAs(start));
}

TEST_CASE("two histories do not influence each other", "[history]") {
    History a{Document(21)};
    History b{Document(21)};
    const auto idA = a.addFeature(boxDraft());
    b.addFeature(boxDraft());
    a.execute(makeSetFeatureValue(*idA, "width", "99"));
    CHECK(b.document().features()[0].params.at("width").expression == "20");
    CHECK(b.position() == 1);
    CHECK(a.position() == 2);
}

TEST_CASE("resetting replaces the document and forgets the steps", "[history]") {
    History history{Document(30)};
    history.addFeature(boxDraft());
    history.beginTransaction(group());
    history.reset(Document(31));
    CHECK_FALSE(history.inTransaction());
    CHECK_FALSE(history.canUndo());
    CHECK(history.document().features().empty());
    CHECK(history.isClean());
}
