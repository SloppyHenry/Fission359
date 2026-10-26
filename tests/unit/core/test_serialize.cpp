// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Document <-> document.json (AP-1.10a): nothing that is content may get lost or changed, and
// no bad file may crash the reader.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <clocale>
#include <memory>
#include <string>

#include "fission/core/commands.h"
#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/serialize.h"
#include "fission/features/builtin.h"

#include "random_editor.h"
#include <nlohmann/json.hpp>

using namespace fission::core;
using fission::core::testing::RandomEditor;
using Json = nlohmann::json;

namespace {

ElementLook sampleLook() {
    ElementLook look;
    look.kind = ElementKind::face;
    look.shape = ElementShape::cylinder;
    look.centroid = {1.0 / 3.0, -2.5e-7, 1234567.891};
    look.direction = {0.0, 0.6, 0.8};
    look.hasDirection = true;
    look.directed = false;
    look.size = 314.159265358979;
    look.radius = 10.0;
    look.neighbours = {"F1/box/top", "F1/box/left"};
    look.neighbourLooks = {{"F1/box/top", ElementShape::plane, {0, 0, 20}, {0, 0, 1}, true, true}};
    return look;
}

// A document that uses every feature type the program knows, a measure with a formula, a text
// with umlauts, holes, colours, references, a palette and a time marker.
Document everythingDocument() {
    FeatureRegistry registry;
    fission::features::registerBuiltinFeatures(registry);
    Document document(77);
    document.title = "Schraubdeckel \xC3\xA4\xC3\xB6 \"Test\"";
    document.setSettings({25.4, {{"black", {1, 2, 3}, false}, {"custom", {250, 128, 0}, true}}});
    document.addParameter({"Breite", "2,5 * 12 mm", QuantityKind::length, "Wand \xC3\xBC"});
    document.addParameter({"Winkel", "45\xC2\xB0", QuantityKind::angle, ""});
    document.addParameter({"Anzahl", "7", QuantityKind::plain, "zweimal"});
    document.addParameter({"Anzahl", "8", QuantityKind::plain, ""}); // repeated on purpose

    std::vector<BodyId> firstBodies;
    for (const std::string& id : registry.ids()) {
        FeatureDraft draft = defaultDraft(*registry.find(id));
        if (!draft.params.empty()) {
            draft.params.begin()->second.expression = "Breite + 1,5";
        }
        if (id == "combine" || id == "split" || id == "place") {
            draft.inputs = firstBodies;
        }
        if (id == "text") {
            draft.texts["text"] = "H\xC3\xA4llo, W\xC3\xB6rld \\ / \" \n";
        }
        draft.refs["target"] = {{ElementKind::face, "F1/box/top", sampleLook()}};
        const auto made = document.addFeature(std::move(draft));
        REQUIRE(made);
        if (firstBodies.size() < 2) {
            for (const BodyId& body : document.findFeature(*made)->outputs) {
                firstBodies.push_back(body);
            }
        }
    }
    document.setFeatureSuppressed(document.features().at(1).id, true);
    document.setFeatureLabel(document.features().at(0).id, "Grundplatte");
    const auto& bodies = document.bodies();
    document.setBodyName(bodies.at(0).id, "Deckel");
    document.setBodyColor(bodies.at(0).id, {255, 0, 17});
    document.setBodyHole(bodies.at(1).id, true);
    document.setBodyVisible(bodies.at(1).id, false);
    document.setBodyCutExceptions(bodies.at(1).id, {bodies.at(0).id});
    document.setRollback(document.features().size() - 1);
    return document;
}

LoadResult load(const std::string& text) {
    return fromJson(text);
}

} // namespace

TEST_CASE("a document with every feature type survives the round trip", "[serialize]") {
    const Document original = everythingDocument();
    REQUIRE(original.features().size() >= 7);

    const std::string text = toJson(original);
    const LoadResult loaded = load(text);

    REQUIRE(loaded.document);
    CHECK(loaded.problems.empty());
    CHECK(loaded.document->sameContentAs(original));
    // Same ids, same numbers, and writing again gives the same text.
    CHECK(loaded.document->features().front().id == original.features().front().id);
    CHECK(toJson(*loaded.document) == text);
}

TEST_CASE("expressions are saved as typed, with comma and units", "[serialize]") {
    const Document loaded = *load(toJson(everythingDocument())).document;
    CHECK(loaded.findParameter("Breite")->expression == "2,5 * 12 mm");
    CHECK(loaded.findParameter("Winkel")->expression == "45\xC2\xB0");
}

TEST_CASE("the result does not depend on the computer's number format", "[serialize]") {
    // The PO's terminal is German; a locale with a decimal comma must not leak into the file.
    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    const char* german = std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
    const Document original = everythingDocument();
    const std::string text = toJson(original);
    const LoadResult loaded = load(text);
    std::setlocale(LC_NUMERIC, saved.c_str());
    (void)german; // not installed everywhere: the test then runs in the plain locale

    REQUIRE(loaded.document);
    CHECK(loaded.document->sameContentAs(original));
    CHECK(text.find("0,3333") == std::string::npos);
}

TEST_CASE("200 random documents come back equal", "[serialize]") {
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        History history{Document(seed)};
        RandomEditor editor(seed * 31337);
        const std::size_t steps = 5 + seed % 40;
        for (std::size_t i = 0; i < steps; ++i) {
            history.execute(editor.next(history.document()));
        }
        const Document& original = history.document();
        const LoadResult loaded = load(toJson(original));
        INFO("seed " << seed);
        REQUIRE(loaded.document);
        CHECK(loaded.problems.empty());
        CHECK(loaded.document->sameContentAs(original));
    }
}

TEST_CASE("a loaded document goes on working and never reuses numbers", "[serialize]") {
    Document original = everythingDocument();
    Document loaded = *load(toJson(original)).document;
    const auto next = loaded.addFeature(fission::core::testing::boxDraft());
    REQUIRE(next);
    CHECK(loaded.findFeature(*next)->number == original.lastFeatureNumber() + 1);
    // New ids are made fresh and do not collide with loaded ones.
    for (const Feature& feature : original.features()) {
        CHECK(feature.id != *next);
    }
}

TEST_CASE("broken files give a problem and no document", "[serialize]") {
    const std::string good = toJson(everythingDocument());

    SECTION("empty, garbage, cut off") {
        for (const std::string& text :
             {std::string(), std::string("not json"), good.substr(0, good.size() / 2),
              std::string("[1, 2]"), std::string("42"), std::string("null")}) {
            const LoadResult result = load(text);
            CHECK_FALSE(result.document);
            CHECK(result.hasFatalProblem());
        }
    }
    SECTION("every cut of the file is survived") {
        for (std::size_t length = 0; length < good.size(); length += 97) {
            const LoadResult result = load(good.substr(0, length));
            CHECK_FALSE(result.document);
        }
    }
    SECTION("a missing required field says which") {
        Json json = Json::parse(good);
        json["features"][2].erase("type");
        const LoadResult result = load(json.dump());
        CHECK_FALSE(result.document);
        REQUIRE(result.problems.size() == 1);
        CHECK(result.problems[0] == LoadProblem{LoadProblemCode::missingField, "/features/2/type"});
    }
    SECTION("a wrong type says which") {
        Json json = Json::parse(good);
        json["parameters"][0]["expression"] = 5;
        const LoadResult result = load(json.dump());
        CHECK_FALSE(result.document);
        REQUIRE(result.problems.size() == 1);
        CHECK(result.problems[0] ==
              LoadProblem{LoadProblemCode::wrongType, "/parameters/0/expression"});
    }
    SECTION("an unknown word in a list") {
        Json json = Json::parse(good);
        json["bodies"][0]["kind"] = "banana";
        const LoadResult result = load(json.dump());
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0).code == LoadProblemCode::unknownValue);
    }
    SECTION("repeated or empty ids") {
        Json json = Json::parse(good);
        json["features"][1]["id"] = json["features"][0]["id"];
        CHECK(load(json.dump()).problems.at(0).code == LoadProblemCode::inconsistent);
        json = Json::parse(good);
        json["bodies"][0]["id"] = "";
        CHECK(load(json.dump()).problems.at(0).code == LoadProblemCode::inconsistent);
        json = Json::parse(good);
        json["rollback"] = 9999;
        CHECK(load(json.dump()).problems.at(0).code == LoadProblemCode::inconsistent);
    }
    SECTION("no format version") {
        Json json = Json::parse(good);
        json.erase("formatVersion");
        CHECK(load(json.dump()).problems.at(0).code == LoadProblemCode::missingField);
        json["formatVersion"] = 0;
        CHECK(load(json.dump()).problems.at(0).code == LoadProblemCode::unsupportedFormat);
    }
}

TEST_CASE("a file from a newer version is read as far as it goes", "[serialize]") {
    Json json = Json::parse(toJson(everythingDocument()));
    json["formatVersion"] = currentFormatVersion + 1;
    json["futureThing"] = {{"a", 1}};
    json["features"][0]["glow"] = true;
    const LoadResult result = load(json.dump());

    REQUIRE(result.document);
    CHECK_FALSE(result.hasFatalProblem());
    const auto has = [&](LoadProblem problem) {
        return std::find(result.problems.begin(), result.problems.end(), problem) !=
               result.problems.end();
    };
    CHECK(has({LoadProblemCode::newerFormat, "/formatVersion"}));
    CHECK(has({LoadProblemCode::unknownField, "/futureThing"}));
    CHECK(has({LoadProblemCode::unknownField, "/features/0/glow"}));
}

TEST_CASE("older files are lifted one version at a time", "[serialize]") {
    // A made-up version 0 that called the title "name" and had no palette; the migration exists
    // only here. It proves the mechanism that every later format change relies on.
    Json json = Json::parse(toJson(everythingDocument()));
    json["formatVersion"] = 0;
    json["name"] = json["title"];
    json.erase("title");
    json["settings"].erase("palette");

    int calls = 0;
    const MigrationTable table{{0, [&](Json& root) {
                                    ++calls;
                                    root["title"] = root["name"];
                                    root.erase("name");
                                    root["settings"]["palette"] = Json::array();
                                }}};
    const LoadResult result = fromJsonWithMigrations(json.dump(), table, 1);

    REQUIRE(result.document);
    CHECK(calls == 1);
    CHECK(result.problems.empty());
    CHECK(result.document->title == everythingDocument().title);

    // No migration for a step: refused, not guessed.
    CHECK(fromJsonWithMigrations(json.dump(), {}, 1).problems.at(0).code ==
          LoadProblemCode::unsupportedFormat);
    // The program's own table is empty at version 1: version 0 is not readable by it.
    CHECK_FALSE(load(json.dump()).document);
}
