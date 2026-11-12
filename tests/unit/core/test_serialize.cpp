// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Document <-> document.json (AP-1.10a): nothing that is content may get lost or changed, and
// no bad file may crash the reader.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <clocale>
#include <functional>
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
    // The sketch step holds one element of every kind, with numbers that do not round nicely.
    for (const Feature& feature : document.features()) {
        if (feature.type != "sketch") {
            continue;
        }
        const FeatureId sketch = feature.id;
        document.addSketchElement(sketch, SketchElementKind::point, {{1.0 / 3.0, -2.5e-7}});
        document.addSketchElement(sketch, SketchElementKind::line, {{0, 0}, {1234567.891, -0.1}});
        document.addSketchElement(sketch, SketchElementKind::circle, {{5, 5}, {5, 12.75}}, true);
        document.addSketchElement(sketch, SketchElementKind::arc, {{0, 0}, {10, 0}, {0, 10}});
        document.addSketchElement(sketch, SketchElementKind::spline,
                                  {{0, 0}, {1, 2}, {3, 1}, {4, 4}});
        document.takeSketchElement(sketch, "L1"); // L1 is gone for good, the next line is L2
        document.addSketchElement(sketch, SketchElementKind::line, {{2, 2}, {3, 3}});
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

// --- sketches (format 3, AP-2.1a) -----------------------------------------------------------

namespace {

const Feature& sketchStep(const Document& document) {
    for (const Feature& feature : document.features()) {
        if (feature.type == "sketch") {
            return feature;
        }
    }
    FAIL("no sketch step");
    return document.features().front();
}

} // namespace

TEST_CASE("sketches survive the round trip with ids, counters and aids", "[serialize][sketch]") {
    const Document original = everythingDocument();
    const SketchData& before = *sketchStep(original).sketch;
    REQUIRE(before.elements.size() == 5);
    CHECK(before.elements.back().id == "L2");

    const LoadResult loaded = load(toJson(original));
    REQUIRE(loaded.document);
    CHECK(loaded.problems.empty());
    const SketchData& after = *sketchStep(*loaded.document).sketch;
    CHECK(after == before);
    CHECK(after.lastNumber[static_cast<std::size_t>(SketchElementKind::line)] == 2);
    CHECK(after.elements[1].construction); // the circle
    CHECK(after.elements[3].points.size() == 4);

    // A new element after loading gets the next number, not a used one.
    Document again = *loaded.document;
    CHECK(*again.addSketchElement(sketchStep(again).id, SketchElementKind::line,
                                  {{0, 0}, {1, 1}}) == "L3");
}

TEST_CASE("only sketch steps carry the field sketch", "[serialize][sketch]") {
    const Json json = Json::parse(toJson(everythingDocument()));
    int sketches = 0;
    for (const Json& feature : json.at("features")) {
        CHECK(feature.contains("sketch") == (feature.at("type") == "sketch"));
        sketches += feature.contains("sketch") ? 1 : 0;
    }
    CHECK(sketches == 1);
    CHECK(json.at("formatVersion") == currentFormatVersion);
}

TEST_CASE("a file of format 2 opens in format 4, a sketch step is an empty sketch",
          "[serialize][sketch]") {
    Json json = Json::parse(toJson(everythingDocument()));
    json["formatVersion"] = 2;
    for (Json& feature : json["features"]) {
        feature.erase("sketch");
    }
    const LoadResult loaded = load(json.dump());
    REQUIRE(loaded.document);
    CHECK(loaded.problems.empty());
    const Feature& step = sketchStep(*loaded.document);
    REQUIRE(step.sketch.has_value());
    CHECK(step.sketch->elements.empty());
    // Written again it is the current format, with the field.
    CHECK(Json::parse(toJson(*loaded.document)).at("formatVersion") == currentFormatVersion);
    CHECK(load(toJson(*loaded.document)).document->sameContentAs(*loaded.document));
}

TEST_CASE("a file of format 1 is lifted through 2 and 3 to 4", "[serialize][sketch]") {
    Json json = Json::parse(toJson(everythingDocument()));
    json["formatVersion"] = 1;
    for (Json& feature : json["features"]) {
        feature.erase("sketch");
    }
    for (Json& parameter : json["parameters"]) {
        parameter.erase("slider");
    }
    const LoadResult loaded = load(json.dump());
    REQUIRE(loaded.document);
    CHECK(loaded.problems.empty());
    CHECK(sketchStep(*loaded.document).sketch.has_value());
    CHECK_FALSE(loaded.document->parameters().front().slider);
}

// --- taken-over edges (AP-2.1f, format 4) ------------------------------------------------------

namespace {

// The document of `everythingDocument` with the sketch's first line linked to an edge.
Document linkedDocument() {
    Document document = everythingDocument();
    const FeatureId step = sketchStep(document).id;
    document.setSketchLink(step, "L2", SketchLink{1, 3});
    return document;
}

} // namespace

TEST_CASE("a link to an edge survives the round trip, and only linked elements have the field",
          "[serialize][sketch][format4]") {
    const Document original = linkedDocument();
    const Json json = Json::parse(toJson(original));
    CHECK(json.at("formatVersion") == currentFormatVersion);
    int links = 0;
    for (const Json& feature : json.at("features")) {
        if (!feature.contains("sketch")) {
            continue;
        }
        for (const Json& element : feature.at("sketch").at("elements")) {
            if (element.contains("link")) {
                ++links;
                CHECK(element.at("id") == "L2");
                CHECK(element.at("link").at("input") == 1);
                CHECK(element.at("link").at("ref") == 3);
            }
        }
    }
    CHECK(links == 1);

    const LoadResult loaded = load(toJson(original));
    REQUIRE(loaded.document);
    CHECK(loaded.problems.empty());
    const SketchElement* line = sketchStep(*loaded.document).sketch->find("L2");
    REQUIRE(line != nullptr);
    CHECK(line->link == SketchLink{1, 3});
    CHECK(loaded.document->sameContentAs(original));
    CHECK_FALSE(sketchStep(*loaded.document).sketch->find("C1")->link.has_value());
}

TEST_CASE("a file of format 3 opens in format 4, nothing is linked",
          "[serialize][sketch][format4]") {
    Json json = Json::parse(toJson(everythingDocument()));
    json["formatVersion"] = 3;
    const LoadResult loaded = load(json.dump());
    REQUIRE(loaded.document);
    CHECK(loaded.problems.empty());
    for (const SketchElement& element : sketchStep(*loaded.document).sketch->elements) {
        CHECK_FALSE(element.link.has_value());
    }
    CHECK(sketchStep(*loaded.document).sketch->elements.size() == 5);
    // Written again it is the current format
    CHECK(Json::parse(toJson(*loaded.document)).at("formatVersion") == currentFormatVersion);
    CHECK(load(toJson(*loaded.document)).document->sameContentAs(*loaded.document));
}

TEST_CASE("a program of format 3 would refuse format 4: the file says so", "[serialize][format4]") {
    // The version number is what older programs check; a file with a link is never written as 3.
    const Json json = Json::parse(toJson(linkedDocument()));
    CHECK(json.at("formatVersion") >= 4);
}

TEST_CASE("a broken link gives a problem and no document", "[serialize][sketch][format4]") {
    const Json good = Json::parse(toJson(linkedDocument()));
    std::size_t at = 0;
    while (good["features"][at].at("type") != "sketch") {
        ++at;
    }
    const auto broken = [&](const std::function<void(Json&)>& change) {
        Json json = good;
        for (Json& element : json["features"][at]["sketch"]["elements"]) {
            if (element.contains("link")) {
                change(element["link"]);
            }
        }
        return load(json.dump());
    };
    CHECK_FALSE(broken([](Json& link) { link["input"] = -1; }).document);
    CHECK_FALSE(broken([](Json& link) { link["ref"] = "x"; }).document);
    CHECK_FALSE(broken([](Json& link) { link.erase("ref"); }).document);
    // an unknown field is only noted
    const LoadResult extra = broken([](Json& link) { link["later"] = 1; });
    REQUIRE(extra.document);
    CHECK(extra.hasFatalProblem() == false);
}

TEST_CASE("broken sketches give a problem and no document", "[serialize][sketch]") {
    const Json good = Json::parse(toJson(everythingDocument()));
    std::size_t at = 0;
    while (good["features"][at].at("type") != "sketch") {
        ++at;
    }
    const std::string base = "/features/" + std::to_string(at) + "/sketch";
    const auto broken = [&](const std::function<void(Json&)>& change) {
        Json json = good;
        change(json["features"][at]["sketch"]);
        return load(json.dump());
    };

    SECTION("an id used twice") {
        const LoadResult result =
            broken([](Json& sketch) { sketch["elements"][1]["id"] = sketch["elements"][0]["id"]; });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0) == LoadProblem{LoadProblemCode::inconsistent, base});
    }
    SECTION("a counter behind an id would give the id out again") {
        const LoadResult result = broken([](Json& sketch) { sketch["lastNumber"]["S"] = 0; });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0).code == LoadProblemCode::inconsistent);
    }
    SECTION("the letter of the id does not fit the kind") {
        const LoadResult result = broken([](Json& sketch) { sketch["elements"][0]["id"] = "L7"; });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0).code == LoadProblemCode::inconsistent);
    }
    SECTION("the wrong number of points for the kind") {
        const LoadResult result =
            broken([](Json& sketch) { sketch["elements"][1]["points"].push_back({1, 2}); });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0).code == LoadProblemCode::inconsistent);
    }
    SECTION("an unknown kind") {
        const LoadResult result =
            broken([](Json& sketch) { sketch["elements"][0]["kind"] = "ellipse"; });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0) ==
              LoadProblem{LoadProblemCode::unknownValue, base + "/elements/0/kind"});
    }
    SECTION("a point that is not two numbers") {
        const LoadResult result = broken(
            [](Json& sketch) { sketch["elements"][1]["points"][0] = Json::array({1, 2, 3}); });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0) ==
              LoadProblem{LoadProblemCode::wrongType, base + "/elements/1/points/0"});
    }
    SECTION("a missing field says which") {
        const LoadResult result = broken([](Json& sketch) { sketch.erase("rules"); });
        CHECK_FALSE(result.document);
        CHECK(result.problems.at(0) == LoadProblem{LoadProblemCode::missingField, base + "/rules"});
    }
    SECTION("a field this version does not know is reported and ignored") {
        const LoadResult result = broken([](Json& sketch) { sketch["grid"] = 5; });
        REQUIRE(result.document);
        CHECK_FALSE(result.hasFatalProblem());
        CHECK(result.problems.at(0) == LoadProblem{LoadProblemCode::unknownField, base + "/grid"});
    }
}
