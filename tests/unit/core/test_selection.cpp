// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Names of faces, edges and corners, and the rules of the selection (AP-1.4a).

#include <catch2/catch_test_macros.hpp>

#include <set>

#include "fission/core/document.h"
#include "fission/core/element_names.h"
#include "fission/core/recompute.h"
#include "fission/core/selection.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using namespace fission::core;

namespace {

struct Scene {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    Document document{5};

    Scene() { fission::features::registerBuiltinFeatures(registry); }

    BodyId add(const char* type, std::map<std::string, std::string> values = {}) {
        FeatureDraft draft = defaultDraft(*registry.find(type));
        for (auto& [name, text] : values) {
            draft.params[name].expression = text;
        }
        const FeatureId id = *document.addFeature(std::move(draft));
        return document.findFeature(id)->outputs.at(0);
    }

    // The names of every body, as the user interface builds them for the current recipe.
    std::map<BodyId, ElementNames> names() {
        const auto result = recompute(document, registry, *kernel, cache);
        REQUIRE(result.has_value());
        std::map<BodyId, ElementNames> all;
        for (const auto& [id, body] : result->bodies) {
            all.emplace(id, ElementNames::build(body.faces, kernel->tessellate(body.solid, {})));
        }
        return all;
    }
};

SelectionItem face(const BodyId& body, const ElementNames& names, const std::string& name) {
    const auto number = names.findFace(name);
    REQUIRE(number.has_value());
    return {body, SelectableKind::face, *number, name};
}

} // namespace

TEST_CASE("names: a box has a name for each of its 6 faces, 12 edges and 8 corners",
          "[selection][naming]") {
    Scene scene;
    const BodyId box = scene.add("box");
    const auto names = scene.names();
    const ElementNames& n = names.at(box);

    CHECK(n.faceCount() == 6);
    CHECK(n.edgeCount() == 12);
    CHECK(n.cornerCount() == 8);

    std::set<std::string> all;
    for (std::uint32_t id = 1; id <= 12; ++id) {
        const std::string& name = n.edgeName(id);
        CHECK_FALSE(name.empty());
        all.insert(name);
    }
    CHECK(all.size() == 12); // no name twice
    for (std::uint32_t id = 1; id <= 8; ++id) {
        CHECK_FALSE(n.cornerName(id).empty());
    }
    CHECK(n.faceName(1).find("/box/") != std::string::npos);
}

TEST_CASE("names: an edge is called after the two faces it joins", "[selection][naming]") {
    Scene scene;
    const BodyId box = scene.add("box");
    const auto names = scene.names();
    const ElementNames& n = names.at(box);

    const std::string top = n.faceName(*n.findFace(n.faceName(2)));
    bool found = false;
    for (std::uint32_t id = 1; id <= 12; ++id) {
        const std::string& name = n.edgeName(id);
        if (name.find(top) != std::string::npos) {
            found = true;
            CHECK(name.rfind("edge[", 0) == 0);
            CHECK(name.find('|') != std::string::npos);
        }
    }
    CHECK(found);
}

TEST_CASE("names: the cylinder's seam and rims have names too", "[selection][naming]") {
    Scene scene;
    const BodyId cylinder = scene.add("cylinder");
    const auto names = scene.names();
    const ElementNames& n = names.at(cylinder);

    CHECK(n.faceCount() == 3);
    // Two rims and the seam; each is named once, none is lost to a clash.
    std::set<std::string> edges;
    for (std::uint32_t id = 1; id <= 3; ++id) {
        if (!n.edgeName(id).empty()) {
            edges.insert(n.edgeName(id));
        }
    }
    CHECK(edges.size() == 3);
    CHECK(n.cornerCount() == 2);
    CHECK_FALSE(n.cornerName(1).empty());
    CHECK_FALSE(n.cornerName(2).empty());
    CHECK(n.cornerName(1) != n.cornerName(2));
}

TEST_CASE("names: a name that is not unique belongs to nobody", "[selection][naming]") {
    // Two faces with the same name, both are let go rather than one chosen at random.
    fission::geo::TriangleMesh mesh;
    const std::vector<BodyFace> faces = {{1, "F1/x/a"}, {2, "F1/x/a"}, {3, "F1/x/b"}};
    const ElementNames n = ElementNames::build(faces, mesh);
    CHECK(n.faceName(1).empty());
    CHECK(n.faceName(2).empty());
    CHECK(n.faceName(3) == "F1/x/b");
    CHECK_FALSE(n.findFace("F1/x/a").has_value());
    CHECK(n.findFace("F1/x/b") == std::optional<std::uint32_t>(3));
    CHECK_FALSE(n.findFace("").has_value());
}

TEST_CASE("selection: click, ⇧-click and click on the background", "[selection]") {
    const BodyId body{"b1"};
    SelectionItem a{body, SelectableKind::face, 1, "F1/box/top"};
    SelectionItem b{body, SelectableKind::face, 2, "F1/box/bottom"};
    Selection selection;
    CHECK(selection.empty());

    CHECK(selection.click(a, false));
    CHECK(selection.items().size() == 1);
    CHECK_FALSE(selection.click(a, false)); // nothing changes: no new revision
    const auto revision = selection.revision();

    CHECK(selection.click(b, true)); // ⇧ adds
    CHECK(selection.items().size() == 2);
    CHECK(selection.contains(a));
    CHECK(selection.contains(b));

    CHECK(selection.click(a, true)); // ⇧ on a chosen one removes it
    CHECK(selection.items().size() == 1);
    CHECK_FALSE(selection.contains(a));
    CHECK(selection.revision() > revision);

    CHECK(selection.click(a, false)); // plain click replaces
    CHECK(selection.items().size() == 1);
    CHECK(selection.contains(a));

    CHECK_FALSE(selection.click(std::nullopt, true)); // ⇧ on the background keeps everything
    CHECK(selection.click(std::nullopt, false));      // plain click on the background clears
    CHECK(selection.empty());
}

TEST_CASE("selection: a double click chooses the whole body", "[selection]") {
    const BodyId body{"b1"};
    Selection selection;
    selection.click(SelectionItem{body, SelectableKind::face, 3, "F1/box/left"}, false);

    CHECK(
        selection.doubleClick(SelectionItem{body, SelectableKind::face, 3, "F1/box/left"}, false));
    REQUIRE(selection.items().size() == 1);
    CHECK(selection.items()[0].kind == SelectableKind::body);
    CHECK(selection.items()[0].body == body);

    // A second body with ⇧ is added, and a further ⇧-double-click on it takes it away again.
    const BodyId other{"b2"};
    selection.doubleClick(SelectionItem{other, SelectableKind::edge, 1, ""}, true);
    CHECK(selection.items().size() == 2);
    selection.doubleClick(SelectionItem{other, SelectableKind::edge, 1, ""}, true);
    CHECK(selection.items().size() == 1);
}

TEST_CASE("selection: the filter decides what can be chosen", "[selection]") {
    const BodyId body{"b1"};
    SelectionItem face{body, SelectableKind::face, 1, "F1/box/top"};
    SelectionItem edge{body, SelectableKind::edge, 1, "edge[a|b]"};
    Selection selection;
    selection.click(face, false);
    selection.click(edge, true);
    REQUIRE(selection.items().size() == 2);

    CHECK(selection.setFilter(SelectionFilter::only(SelectableKind::edge)));
    REQUIRE(selection.items().size() == 1);
    CHECK(selection.contains(edge));

    // A face does not react while only edges can be chosen: it counts as the background.
    CHECK_FALSE(selection.click(face, true));
    CHECK(selection.click(face, false)); // clears, the edge is let go
    CHECK(selection.empty());

    CHECK(selection.rubberBand({face, edge}, false));
    CHECK(selection.items().size() == 1);
    CHECK_FALSE(selection.setFilter(SelectionFilter::only(SelectableKind::edge)));
    CHECK(SelectionFilter::all().allows(SelectableKind::vertex));
    CHECK_FALSE(SelectionFilter::only(SelectableKind::body).allows(SelectableKind::face));
}

TEST_CASE("selection: a rubber band replaces or extends without duplicates", "[selection]") {
    const BodyId body{"b1"};
    SelectionItem a{body, SelectableKind::face, 1, "x"};
    SelectionItem b{body, SelectableKind::face, 2, "y"};
    SelectionItem c{body, SelectableKind::face, 3, "z"};
    Selection selection;
    selection.click(a, false);

    CHECK(selection.rubberBand({b, c, b}, false));
    CHECK(selection.items().size() == 2);
    CHECK_FALSE(selection.contains(a));
    CHECK(selection.rubberBand({a, b}, true));
    CHECK(selection.items().size() == 3);
}

TEST_CASE("selection: stays chosen when a value changes, as long as the face exists",
          "[selection][naming]") {
    Scene scene;
    const BodyId box = scene.add("box", {{"width", "20"}});
    auto before = scene.names();

    Selection selection;
    selection.click(face(box, before.at(box), "F1/box/top"), false);
    // Not what the name's number is in this mesh, but what the name is: that is what counts.
    const FeatureId feature = scene.document.features().front().id;
    REQUIRE(scene.document.setFeatureValue(feature, "width", "35"));
    const auto after = scene.names();

    CHECK(selection.reconcile(after) == 0);
    REQUIRE(selection.items().size() == 1);
    const SelectionItem& item = selection.items()[0];
    CHECK(item.name == "F1/box/top");
    CHECK(item.number == after.at(box).findFace("F1/box/top").value());
}

TEST_CASE("selection: lets go of what cannot be found for certain", "[selection][naming]") {
    const BodyId gone{"gone"};
    const BodyId body{"b1"};
    fission::geo::TriangleMesh mesh;
    const std::map<BodyId, ElementNames> now{
        {body, ElementNames::build({{1, "F1/x/a"}, {2, "F1/x/b"}}, mesh)}};

    Selection selection;
    selection.set({SelectionItem{gone, SelectableKind::body, 0, ""},
                   SelectionItem{body, SelectableKind::body, 0, ""},
                   SelectionItem{body, SelectableKind::face, 1, "F1/x/a"},
                   SelectionItem{body, SelectableKind::face, 7, "F1/x/removed"},
                   SelectionItem{body, SelectableKind::face, 9, ""}});
    REQUIRE(selection.size() == 5);

    CHECK(selection.reconcile(now) == 3); // body gone, name gone, never had a name
    REQUIRE(selection.size() == 2);
    CHECK(selection.items()[0].kind == SelectableKind::body);
    CHECK(selection.items()[1].name == "F1/x/a");
}
