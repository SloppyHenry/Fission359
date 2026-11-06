// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Named measures (AP-2.9): renaming everywhere in one step, where a measure is used, the slider
// range, and the file format 2 with its way up from format 1.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fission/core/commands.h"
#include "fission/core/document.h"
#include "fission/core/expression.h"
#include "fission/core/history.h"
#include "fission/core/serialize.h"

#include <nlohmann/json.hpp>

using namespace fission::core;

namespace {

FeatureDraft boxWith(const std::string& width, const std::string& depth = "20") {
    FeatureDraft draft;
    draft.type = "box";
    draft.params["width"] = {width, QuantityKind::length};
    draft.params["depth"] = {depth, QuantityKind::length};
    draft.newBodies = 1;
    return draft;
}

} // namespace

TEST_CASE("rename in a formula: only whole names, whatever the case", "[measures]") {
    CHECK(renameInExpression("Breite", "Breite", "Halterbreite") == "Halterbreite");
    CHECK(renameInExpression("Breite/2", "Breite", "B") == "B/2");
    CHECK(renameInExpression("Breite2 + Breite", "Breite", "B") == "Breite2 + B");
    CHECK(renameInExpression("Halterbreite - Breite", "Breite", "B") == "Halterbreite - B");
    CHECK(renameInExpression("breite*BREITE", "Breite", "B") == "B*B");
    CHECK(renameInExpression("min(Breite; 10 mm) + Breite", "Breite", "W") == "min(W; 10 mm) + W");
    CHECK(renameInExpression("  Breite  /  2 ", "Breite", "W") == "  W  /  2 "); // spaces stay
    CHECK(renameInExpression("Größe*2", "groesse", "Maß") == "Maß*2");           // umlauts fold
    CHECK(renameInExpression("25 mm", "Breite", "W") == "25 mm");
    CHECK(renameInExpression("", "Breite", "W").empty());
    // A formula that does not parse is not touched: nothing is guessed.
    CHECK(renameInExpression("Breite +", "Breite", "W") == "Breite +");
    CHECK(renameInExpression("Breite $", "Breite", "W") == "Breite $");
}

TEST_CASE("rename everywhere: steps, other measures, one undo step", "[measures]") {
    History history{Document(3)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    history.execute(makeAddParameter({"Breite2", "Breite * 2", QuantityKind::length, ""}));
    history.execute(makeAddParameter({"Halb", "Breite / 2", QuantityKind::length, ""}));
    const auto first = history.addFeature(boxWith("Breite", "Breite2"));
    const auto second = history.addFeature(boxWith("Breite + 1", "20"));
    REQUIRE(first);
    REQUIRE(second);
    const Document before = history.document();

    REQUIRE(history.execute(makeRenameMeasureEverywhere("Breite", "Halterbreite")));
    const Document& after = history.document();
    CHECK(after.parameters()[0].name == "Halterbreite");
    CHECK(after.parameters()[1].expression == "Halterbreite * 2");
    CHECK(after.parameters()[2].expression == "Halterbreite / 2");
    CHECK(after.findFeature(*first)->params.at("width").expression == "Halterbreite");
    CHECK(after.findFeature(*first)->params.at("depth").expression == "Breite2"); // another name
    CHECK(after.findFeature(*second)->params.at("width").expression == "Halterbreite + 1");

    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
    REQUIRE(history.redo());
    CHECK(history.document().parameters()[0].name == "Halterbreite");
    CHECK(history.document().findFeature(*second)->params.at("width").expression ==
          "Halterbreite + 1");
    CHECK_FALSE(history.canRedo());
}

TEST_CASE("rename everywhere: the formulas still give the same numbers", "[measures]") {
    History history{Document(4)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    history.execute(makeAddParameter({"Halb", "Breite / 2", QuantityKind::length, ""}));
    const auto resolved = [&] { return resolveMeasures(history.document().measureDefinitions()); };
    const double halfBefore = resolved().find("Halb")->value->value;
    REQUIRE(history.execute(makeRenameMeasureEverywhere("Breite", "B")));
    REQUIRE(resolved().find("Halb")->value);
    CHECK(resolved().find("Halb")->value->value == halfBefore);
    CHECK_FALSE(resolved().find("Halb")->error);
}

TEST_CASE("rename everywhere: ten formulas at once", "[measures]") {
    History history{Document(5)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    for (int i = 0; i < 10; ++i) {
        REQUIRE(history.addFeature(
            boxWith("Breite+" + std::to_string(i), "Breite*" + std::to_string(i + 1))));
    }
    const Document before = history.document();
    REQUIRE(history.execute(makeRenameMeasureEverywhere("Breite", "Wand")));
    for (const Feature& feature : history.document().features()) {
        CHECK(feature.params.at("width").expression.starts_with("Wand+"));
        CHECK(feature.params.at("depth").expression.starts_with("Wand*"));
    }
    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
}

TEST_CASE("rename everywhere: wrong names are refused and change nothing", "[measures]") {
    History history{Document(6)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    history.execute(makeAddParameter({"Hoehe", "10", QuantityKind::length, ""}));
    const auto step = history.addFeature(boxWith("Breite"));
    REQUIRE(step);
    const Document before = history.document();
    CHECK_FALSE(history.execute(makeRenameMeasureEverywhere("Breite", "")));
    CHECK_FALSE(history.execute(makeRenameMeasureEverywhere("Breite", "2Breite")));
    CHECK_FALSE(history.execute(makeRenameMeasureEverywhere("Breite", "zwei Worte")));
    CHECK_FALSE(history.execute(makeRenameMeasureEverywhere("Breite", "Höhe"))); // = Hoehe
    CHECK_FALSE(history.execute(makeRenameMeasureEverywhere("Gibtsnicht", "X")));
    CHECK(history.document().sameContentAs(before));
    // Only another spelling of its own name is fine.
    CHECK(history.execute(makeRenameMeasureEverywhere("Breite", "BREITE")));
    CHECK(history.document().findFeature(*step)->params.at("width").expression == "BREITE");
}

TEST_CASE("where a measure is used", "[measures]") {
    Document document(7);
    document.addParameter({"Breite", "30", QuantityKind::length, ""});
    document.addParameter({"Halb", "Breite / 2", QuantityKind::length, ""});
    document.addParameter({"Frei", "5", QuantityKind::length, ""});
    document.addFeature(boxWith("Breite"));
    document.addFeature(boxWith("10", "Halb"));
    document.addFeature(boxWith("Breite + Halb", "Breite"));
    const auto use = document.usesOfParameter("breite");
    CHECK(use.steps == std::vector<std::size_t>{0, 2});
    CHECK(use.measures == std::vector<std::size_t>{1});
    const auto none = document.usesOfParameter("Frei");
    CHECK(none.steps.empty());
    CHECK(none.measures.empty());
}

TEST_CASE("slider range of a measure: command, undo, roundtrip", "[measures]") {
    History history{Document(8)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    const Document before = history.document();
    const SliderRange range{10.0, 80.0, 0.5};
    REQUIRE(history.execute(makeSetParameterSlider("Breite", range)));
    REQUIRE(history.document().parameters()[0].slider);
    CHECK(*history.document().parameters()[0].slider == range);

    const LoadResult loaded = fromJson(toJson(history.document()));
    REQUIRE(loaded.document);
    CHECK_FALSE(loaded.hasFatalProblem());
    CHECK(loaded.document->sameContentAs(history.document()));
    CHECK(*loaded.document->parameters()[0].slider == range);

    REQUIRE(history.undo());
    CHECK(history.document().sameContentAs(before));
    CHECK_FALSE(history.document().parameters()[0].slider);
    REQUIRE(history.redo());
    // Taking it away again.
    REQUIRE(history.execute(makeSetParameterSlider("Breite", std::nullopt)));
    CHECK_FALSE(history.document().parameters()[0].slider);
}

TEST_CASE("slider range: ranges that cannot work are refused", "[measures]") {
    History history{Document(9)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    CHECK_FALSE(history.execute(makeSetParameterSlider("Breite", SliderRange{10, 10, 1})));
    CHECK_FALSE(history.execute(makeSetParameterSlider("Breite", SliderRange{10, 5, 1})));
    CHECK_FALSE(history.execute(makeSetParameterSlider("Breite", SliderRange{0, 10, 0})));
    CHECK_FALSE(history.execute(makeSetParameterSlider("Breite", SliderRange{0, 10, -1})));
    CHECK_FALSE(history.execute(makeSetParameterSlider("Unbekannt", SliderRange{0, 10, 1})));
    CHECK_FALSE(history.document().parameters()[0].slider);
}

TEST_CASE("a file of format 1 opens in the current format without slider",
          "[measures][serialize]") {
    const std::string version1 = R"({
      "formatVersion": 1, "title": "Alt",
      "settings": {"defaultUnitMillimetres": 1.0, "palette": []},
      "parameters": [{"name": "Breite", "expression": "30", "kind": "length", "comment": "alt"}],
      "features": [], "bodies": [], "lastFeatureNumber": 0, "rollback": null })";
    const LoadResult loaded = fromJson(version1);
    REQUIRE(loaded.document);
    CHECK_FALSE(loaded.hasFatalProblem());
    REQUIRE(loaded.document->parameters().size() == 1);
    CHECK(loaded.document->parameters()[0].comment == "alt");
    CHECK_FALSE(loaded.document->parameters()[0].slider);
    // Written again it is the current format and reads back the same.
    const std::string text = toJson(*loaded.document);
    CHECK(nlohmann::json::parse(text).at("formatVersion") == currentFormatVersion);
    const LoadResult again = fromJson(text);
    REQUIRE(again.document);
    CHECK(again.document->sameContentAs(*loaded.document));
}

TEST_CASE("a damaged slider in a file is a problem, not a crash", "[measures][serialize]") {
    History history{Document(10)};
    history.execute(makeAddParameter({"Breite", "30", QuantityKind::length, ""}));
    nlohmann::json json = nlohmann::json::parse(toJson(history.document()));
    for (const nlohmann::json& bad :
         {nlohmann::json("text"), nlohmann::json({{"min", 5}, {"max", 1}, {"step", 1}}),
          nlohmann::json({{"min", 0}, {"max", 1}}), nlohmann::json(7)}) {
        json["parameters"][0]["slider"] = bad;
        const LoadResult loaded = fromJson(json.dump());
        CHECK(loaded.hasFatalProblem());
    }
}
