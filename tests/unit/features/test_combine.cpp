// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The combine step (AP-1.7a, ADR-0011): a result that keeps the names of its inputs, the
// derived "consumed" state of bodies, and a recompute that redoes only what changed.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>
#include <set>

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
};

std::set<std::string> namesOf(const BodyResult& body) {
    std::set<std::string> names;
    for (const BodyFace& face : body.faces) {
        names.insert(face.name);
    }
    return names;
}

} // namespace

TEST_CASE("combine: a hole cut with a step keeps the names of box and cylinder", "[combine]") {
    World world;
    const FeatureId box =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const FeatureId pin = world.add("cylinder", {{"radius", "3"}, {"height", "30"}, {"z", "-10"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(pin)}, {{"mode", "subtract"}});

    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(cut);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    REQUIRE(state->description.has_value());
    CHECK(state->description->key == "combine.subtract");
    REQUIRE(state->description->values.size() == 1);
    CHECK(state->description->values[0].value == Approx(1.0));

    const BodyResult& combined = result.bodies.at(world.bodyOf(cut));
    const fission::geo::SolidReport report = world.kernel->measure(combined.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(16000.0 - std::numbers::pi * 9.0 * 10.0).epsilon(1e-6));
    const std::set<std::string> names = namesOf(combined);
    CHECK(names.count("F1/box/top") == 1);
    CHECK(names.count("F2/cylinder/side") == 1);
    CHECK(names.count("F3/combine/F1/box/top") == 0);
}

TEST_CASE("combine: consumed bodies are derived from the inputs of steps", "[combine]") {
    World world;
    const FeatureId box = world.add("box");
    const FeatureId pin = world.add("cylinder");
    const BodyId boxBody = world.bodyOf(box);
    const BodyId pinBody = world.bodyOf(pin);
    CHECK_FALSE(world.document.consumerOf(boxBody).has_value());

    const FeatureId cut = world.add("combine", {}, {boxBody, pinBody}, {{"mode", "subtract"}});
    CHECK(world.document.consumerOf(boxBody) == cut);
    CHECK(world.document.consumerOf(pinBody) == cut);
    CHECK_FALSE(world.document.consumerOf(world.bodyOf(cut)).has_value());

    world.document.setFeatureSuppressed(cut, true);
    CHECK_FALSE(world.document.consumerOf(boxBody).has_value());
}

TEST_CASE("combine: tools can be added and taken away, bad ones are refused", "[combine]") {
    World world;
    const FeatureId box = world.add("box");
    const FeatureId pin = world.add("cylinder");
    const FeatureId other = world.add("cylinder");
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(pin)}, {{"mode", "subtract"}});

    CHECK(world.document.setFeatureInputs(
        cut, {world.bodyOf(box), world.bodyOf(pin), world.bodyOf(other)}));
    CHECK(world.document.findFeature(cut)->inputs.size() == 3);
    // The result of the step itself, or an unknown body, cannot be an input.
    CHECK_FALSE(world.document.setFeatureInputs(cut, {world.bodyOf(cut)}));
    CHECK_FALSE(world.document.setFeatureInputs(cut, {BodyId{"nobody"}}));
    CHECK(world.document.findFeature(cut)->inputs.size() == 3);
}

TEST_CASE("combine: an unchanged recipe is remembered, a changed tool redoes only the cut",
          "[combine]") {
    World world;
    const FeatureId box = world.add("box");
    const FeatureId pin = world.add("cylinder", {{"radius", "3"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(pin)}, {{"mode", "subtract"}});

    CHECK(world.run().executed == 3);
    CHECK(world.run().executed == 0);

    world.document.setFeatureValue(pin, "radius", "4");
    const RecomputeResult again = world.run();
    CHECK(again.executed == 2); // the cylinder and the cut, not the box
    const BodyResult& combined = again.bodies.at(world.bodyOf(cut));
    CHECK(world.kernel->measure(combined.solid).volume ==
          Approx(8000.0 - std::numbers::pi * 16.0 * 20.0).epsilon(1e-6));
}

TEST_CASE("combine: intersecting bodies that do not touch is an error that helps", "[combine]") {
    World world;
    const FeatureId box = world.add("box", {{"x", "0"}});
    const FeatureId away = world.add("box", {{"x", "500"}});
    const FeatureId common =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(away)}, {{"mode", "intersect"}});
    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(common);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::combineEmpty);
}

TEST_CASE("combine: a cutter chosen by hand that misses the body is an error that helps",
          "[combine]") {
    World world;
    const FeatureId box = world.add("box", {{"x", "0"}});
    const FeatureId away = world.add("cylinder", {{"x", "500"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(box), world.bodyOf(away)}, {{"mode", "subtract"}});
    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(cut);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::combineNoOverlap);

    // A hole the program put down may miss: that is no error (it has no cut then).
    World other;
    const FeatureId plate = other.add("box", {{"x", "0"}});
    const FeatureId pin = other.add("cylinder", {{"x", "500"}});
    const FeatureId quiet = other.add("combine", {}, {other.bodyOf(plate), other.bodyOf(pin)},
                                      {{"mode", "subtract"}, {"auto", "yes"}});
    CHECK(other.run().state(quiet)->status == FeatureStatus::ok);
}
