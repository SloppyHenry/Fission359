// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The names and looks of a body's faces and edges, and the memory of used-up elements
// (AP-2.10a, ADR-0003 points 2, 3 and 5).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "fission/core/body_looks.h"
#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;

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

    RecomputeResult run() {
        auto result = recompute(document, registry, *kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }

    BodyLooks looksOf(const BodyResult& body) const {
        return describeBody(body.faces, kernel->signatures(body.solid));
    }
};

const NamedLook* find(const std::vector<NamedLook>& list, const std::string& name) {
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const NamedLook& entry) { return entry.name == name; });
    return found == list.end() ? nullptr : &*found;
}

const ConsumedElement* find(const std::vector<ConsumedElement>& list, ElementKind kind,
                            const std::string& name) {
    const auto found = std::find_if(list.begin(), list.end(), [&](const ConsumedElement& c) {
        return c.kind == kind && c.name == name;
    });
    return found == list.end() ? nullptr : &*found;
}

} // namespace

TEST_CASE("looks: a box has named faces and edges with golden looks", "[looks][golden]") {
    World world;
    const FeatureId box =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const RecomputeResult result = world.run();
    const BodyLooks looks = world.looksOf(result.bodies.at(world.bodyOf(box)));

    REQUIRE(looks.faces.size() == 6);
    REQUIRE(looks.edges.size() == 12);
    CHECK(looks.diagonal > 0.0);

    const NamedLook* top = find(looks.faces, "F1/box/top");
    REQUIRE(top != nullptr);
    CHECK(top->look.kind == ElementKind::face);
    CHECK(top->look.shape == ElementShape::plane);
    CHECK(top->look.centroid.z == Approx(10.0));
    CHECK(top->look.direction.z == Approx(1.0));
    CHECK(top->look.directed);
    CHECK(top->look.size == Approx(1600.0));
    REQUIRE(top->look.neighbours.size() == 4);
    CHECK(std::is_sorted(top->look.neighbours.begin(), top->look.neighbours.end()));
    CHECK(top->look.neighbourLooks.size() == 4);

    // An edge is named after the two faces it joins (the same rule as the selection).
    const NamedLook* edge = find(looks.edges, "edge[F1/box/right|F1/box/top]");
    REQUIRE(edge != nullptr);
    CHECK(edge->look.kind == ElementKind::edge);
    CHECK(edge->look.shape == ElementShape::line);
    CHECK(edge->look.size == Approx(40.0));
    CHECK(edge->look.neighbours == std::vector<std::string>{"F1/box/right", "F1/box/top"});
}

TEST_CASE("looks: a name that two faces share belongs to nobody", "[looks]") {
    fission::geo::BodySignatures sigs;
    for (std::uint32_t id = 1; id <= 2; ++id) {
        fission::geo::ElementSignature face;
        face.id = id;
        face.size = 1.0;
        sigs.faces.push_back(face);
    }
    const BodyLooks looks = describeBody({{1, "F1/box/top"}, {2, "F1/box/top"}}, sigs);
    REQUIRE(looks.faces.size() == 2);
    CHECK(looks.faces[0].name.empty());
    CHECK(looks.faces[1].name.empty());
}

TEST_CASE("consumed: a body from nothing remembers nothing", "[consumed]") {
    World world;
    const FeatureId box = world.add("box");
    const RecomputeResult result = world.run();
    CHECK(result.bodies.at(world.bodyOf(box)).consumed.empty());
}

TEST_CASE("consumed: a cut remembers the pin's faces and edges with their looks", "[consumed]") {
    World world;
    const FeatureId box =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const FeatureId pin = world.add("cylinder", {{"radius", "3"}, {"height", "30"}, {"z", "-10"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(pin)}, {{"mode", "subtract"}});
    const RecomputeResult result = world.run();
    const BodyResult& body = result.bodies.at(world.bodyOf(cut));

    const ConsumedElement* top = find(body.consumed, ElementKind::face, "F2/cylinder/top");
    REQUIRE(top != nullptr);
    CHECK(top->step == cut);
    CHECK(top->look.shape == ElementShape::plane);
    CHECK(top->look.centroid.z == Approx(20.0)); // the pin reaches from z = -10 to 20
    CHECK(find(body.consumed, ElementKind::face, "F2/cylinder/bottom") != nullptr);
    CHECK(find(body.consumed, ElementKind::face, "F2/cylinder/side") == nullptr); // still there

    const ConsumedElement* rim =
        find(body.consumed, ElementKind::edge, "edge[F2/cylinder/side|F2/cylinder/top]");
    REQUIRE(rim != nullptr);
    CHECK(rim->look.shape == ElementShape::circle);
    CHECK(rim->look.radius == Approx(3.0));
    // Nothing that is still in the body is remembered as used up.
    const BodyLooks now = world.looksOf(body);
    for (const ConsumedElement& gone : body.consumed) {
        CHECK(find(gone.kind == ElementKind::face ? now.faces : now.edges, gone.name) == nullptr);
    }
}

TEST_CASE("consumed: the memory is inherited and an undone step forgets it", "[consumed]") {
    World world;
    const FeatureId box =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const FeatureId pin = world.add("cylinder", {{"radius", "3"}, {"height", "30"}, {"z", "-10"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(pin)}, {{"mode", "subtract"}});
    const FeatureId second =
        world.add("cylinder", {{"radius", "2"}, {"height", "30"}, {"z", "-10"}, {"x", "10"}});
    const FeatureId again =
        world.add("combine", {}, {world.bodyOf(cut), world.bodyOf(second)}, {{"mode", "subtract"}});
    const RecomputeResult result = world.run();
    const BodyResult& body = result.bodies.at(world.bodyOf(again));
    const ConsumedElement* first = find(body.consumed, ElementKind::face, "F2/cylinder/top");
    const ConsumedElement* later = find(body.consumed, ElementKind::face, "F4/cylinder/top");
    REQUIRE(first != nullptr);
    REQUIRE(later != nullptr);
    CHECK(first->step == cut); // the first step stays the one that used it up
    CHECK(later->step == again);

    // Switching the second cut off: its body has no trace of it.
    world.document.setFeatureSuppressed(again, true);
    const RecomputeResult without = world.run();
    CHECK(without.bodies.count(world.bodyOf(again)) == 0);
    const BodyResult& cutBody = without.bodies.at(world.bodyOf(cut));
    CHECK(find(cutBody.consumed, ElementKind::face, "F4/cylinder/top") == nullptr);
}

TEST_CASE("consumed: merging a list never repeats a name", "[consumed]") {
    std::vector<ConsumedElement> list;
    ConsumedElement a;
    a.name = "F1/box/top";
    a.step = FeatureId{"F1"};
    ConsumedElement b = a;
    b.step = FeatureId{"F9"};
    mergeConsumed(list, {a});
    mergeConsumed(list, {b});
    REQUIRE(list.size() == 1);
    CHECK(list.front().step == a.step);
    ConsumedElement edge = a;
    edge.kind = ElementKind::edge; // the same name as an edge is another element
    mergeConsumed(list, {edge});
    CHECK(list.size() == 2);
}
