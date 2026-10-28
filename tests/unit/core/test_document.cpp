// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <set>

#include "fission/core/document.h"
#include "fission/core/feature_type.h"

using namespace fission::core;

namespace {

FeatureDraft boxDraft(const char* width = "20") {
    FeatureDraft draft;
    draft.type = "box";
    draft.params["width"] = {width, QuantityKind::length};
    draft.newBodies = 1;
    return draft;
}

} // namespace

TEST_CASE("features get stable ids, numbers that are never reused, and their bodies",
          "[document]") {
    Document document(7);
    const auto first = document.addFeature(boxDraft());
    const auto second = document.addFeature(boxDraft());
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->value.size() == 36);
    CHECK(*first != *second);
    CHECK(document.findFeature(*first)->number == 1);
    CHECK(document.findFeature(*second)->number == 2);
    REQUIRE(document.bodies().size() == 2);
    CHECK(document.findFeature(*first)->outputs.at(0) == document.bodies()[0].id);
    CHECK(document.bodies()[0].createdBy == *first);

    // Removing the first step does not free its number.
    REQUIRE(document.removeFeature(*first));
    const auto third = document.addFeature(boxDraft());
    CHECK(document.findFeature(*third)->number == 3);
    CHECK(document.bodies().size() == 2); // its body went with it, one new one came
}

TEST_CASE("the same seed gives the same ids, two documents never share state", "[document]") {
    Document a(42);
    Document b(42);
    Document c(43);
    const auto idA = a.addFeature(boxDraft());
    const auto idB = b.addFeature(boxDraft());
    const auto idC = c.addFeature(boxDraft());
    CHECK(*idA == *idB);
    CHECK(*idA != *idC);

    // Changing one document leaves the other alone.
    REQUIRE(a.setFeatureValue(*idA, "width", "35"));
    CHECK(a.findFeature(*idA)->params.at("width").expression == "35");
    CHECK(b.findFeature(*idB)->params.at("width").expression == "20");
}

TEST_CASE("random ids do not repeat", "[document]") {
    IdSource source;
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i) {
        CHECK(seen.insert(source.nextUuid()).second);
    }
}

TEST_CASE("a copy is a snapshot", "[document]") {
    Document document(1);
    const auto id = document.addFeature(boxDraft());
    const Document snapshot = document;
    REQUIRE(document.setFeatureValue(*id, "width", "99"));
    document.addParameter({"Breite", "50", QuantityKind::length, ""});
    CHECK(snapshot.findFeature(*id)->params.at("width").expression == "20");
    CHECK(snapshot.parameters().empty());
    CHECK(document.revision() > snapshot.revision());
}

TEST_CASE("revision grows with every real change", "[document]") {
    Document document(1);
    const auto id = document.addFeature(boxDraft());
    auto revision = document.revision();
    document.setFeatureValue(*id, "width", "20"); // same text: nothing changed
    CHECK(document.revision() == revision);
    document.setFeatureValue(*id, "width", "21");
    CHECK(document.revision() > revision);
    revision = document.revision();
    CHECK_FALSE(document.setFeatureValue(*id, "nonsense", "1"));
    CHECK(document.revision() == revision);
}

TEST_CASE("a step can only use bodies of earlier steps", "[document]") {
    Document document(1);
    const auto first = document.addFeature(boxDraft());
    const BodyId body = document.findFeature(*first)->outputs.at(0);

    FeatureDraft user;
    user.type = "move";
    user.inputs = {body};
    REQUIRE(document.addFeature(user));

    // Before the step that makes the body: refused, so the recipe can never loop.
    CHECK_FALSE(document.addFeature(user, 0));
    // A body that does not exist: refused.
    FeatureDraft ghost;
    ghost.type = "move";
    ghost.inputs = {BodyId{"nope"}};
    CHECK_FALSE(document.addFeature(ghost));
    CHECK(document.features().size() == 2);
}

TEST_CASE("insertion at a position and dependents", "[document]") {
    Document document(1);
    const auto a = document.addFeature(boxDraft());
    const BodyId bodyA = document.findFeature(*a)->outputs.at(0);

    FeatureDraft usesA;
    usesA.type = "move";
    usesA.inputs = {bodyA};
    usesA.newBodies = 0;
    const auto b = document.addFeature(usesA);
    const auto c = document.addFeature(boxDraft());

    const auto inserted = document.addFeature(boxDraft(), 1);
    REQUIRE(inserted);
    CHECK(document.indexOf(*inserted) == 1);
    CHECK(document.indexOf(*b) == 2);
    CHECK(document.indexOf(*c) == 3);

    CHECK(document.dependentsOf(*a) == std::vector<FeatureId>{*b});
    CHECK(document.dependentsOf(*c).empty());
}

TEST_CASE("named measures are kept as typed, mistakes included", "[document]") {
    Document document(1);
    document.addParameter({"Breite", "50", QuantityKind::length, "Außenmaß"});
    document.addParameter({"breite", "60", QuantityKind::length, ""}); // same name, kept
    document.addParameter({"Lochabstand", "Breite - 10 mm", QuantityKind::length, ""});
    CHECK(document.parameters().size() == 3);
    CHECK(document.findParameter("BREITE")->comment == "Außenmaß");
    CHECK(document.setParameterExpression("Breite", "55"));
    CHECK(document.removeParameter("breite"));
    CHECK(document.removeParameter("breite")); // the second one
    CHECK_FALSE(document.removeParameter("breite"));
    CHECK(document.renameParameter("Lochabstand", "Abstand"));

    const auto definitions = document.measureDefinitions();
    REQUIRE(definitions.size() == 1);
    CHECK(definitions[0].name == "Abstand");
    const auto resolution = resolveMeasures(definitions);
    // "Breite" is gone, so the formula has an unknown name: reported, not hidden.
    CHECK(resolution.results[0].error->code == ExpressionErrorCode::unknownName);
}

TEST_CASE("body properties", "[document]") {
    Document document(1);
    const auto id = document.addFeature(boxDraft());
    const BodyId body = document.findFeature(*id)->outputs.at(0);
    CHECK(document.setBodyName(body, "Deckel"));
    CHECK(document.setBodyColor(body, {255, 0, 0}));
    CHECK(document.setBodyVisible(body, false));
    const Body* found = document.findBody(body);
    REQUIRE(found != nullptr);
    CHECK(found->name == "Deckel");
    CHECK(found->color == Color{255, 0, 0});
    CHECK_FALSE(found->visible);
    CHECK_FALSE(document.setBodyName(BodyId{"x"}, "y"));
}

TEST_CASE("references are plain data and compare by value", "[document]") {
    PersistentRef a;
    a.name = "F1/box/top";
    a.look.shape = ElementShape::plane;
    a.look.neighbours = {"F1/box/front"};
    PersistentRef b = a;
    CHECK(a == b);
    b.look.size = 400.0;
    CHECK_FALSE(a == b);

    Document document(1);
    const auto id = document.addFeature(boxDraft());
    document.setFeatureRefs(*id, "face", {a});
    CHECK(document.findFeature(*id)->refs.at("face").at(0) == a);
}

TEST_CASE("the feature registry refuses a second type of the same name", "[registry]") {
    class Dummy final : public FeatureType {
    public:
        std::string_view id() const override { return "dummy"; }
        int version() const override { return 1; }
        std::vector<ParamSpec> parameters() const override { return {}; }
        std::optional<FeatureIssue> validate(const ResolvedParams&) const override { return {}; }
        FeatureResult execute(const ExecuteInput&, const ExecuteContext&) const override {
            return {};
        }
        Description describe(const ResolvedParams&) const override { return {"dummy", {}}; }
        std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
    };
    FeatureRegistry registry;
    CHECK(registry.add(std::make_unique<Dummy>()));
    CHECK_FALSE(registry.add(std::make_unique<Dummy>()));
    CHECK_FALSE(registry.add(nullptr));
    CHECK(registry.find("dummy") != nullptr);
    CHECK(registry.find("nothing") == nullptr);
    CHECK(registry.ids() == std::vector<std::string>{"dummy"});
}

TEST_CASE("the naming contract: every face has exactly one role", "[registry]") {
    FeatureOutput output;
    fission::geo::SolidReport report;
    report.faces = 3;

    output.faces = {{1, "bottom"}, {2, "top"}, {3, "side"}};
    CHECK_FALSE(checkNamingContract(output, report));

    output.faces = {{1, "bottom"}, {2, "top"}}; // one face unnamed
    CHECK(checkNamingContract(output, report)->code == FeatureIssueCode::namingContractViolated);
    output.faces = {{1, "bottom"}, {1, "top"}, {3, "side"}}; // same face twice
    CHECK(checkNamingContract(output, report));
    output.faces = {{1, "bottom"}, {2, "bottom"}, {3, "side"}}; // same role twice
    CHECK(checkNamingContract(output, report));
    output.faces = {{1, "bottom"}, {2, "top"}, {4, "side"}}; // a face that is not there
    CHECK(checkNamingContract(output, report));
    output.faces = {{1, ""}, {2, "top"}, {3, "side"}}; // empty role
    CHECK(checkNamingContract(output, report));

    Feature feature;
    feature.number = 5;
    feature.type = "hole";
    CHECK(elementName(feature, "wall") == "F5/hole/wall");
}
