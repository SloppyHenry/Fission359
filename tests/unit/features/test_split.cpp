// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Splitting a body at a plane and putting the parts where they are wanted (AP-1.7e, ADR-0011):
// golden values of the two parts, the names of the cut faces, errors that help.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
                  std::vector<BodyId> inputs = {}) {
        FeatureDraft draft = defaultDraft(*registry.find(type));
        for (const auto& [name, text] : values) {
            draft.params.at(name).expression = text;
        }
        draft.inputs = std::move(inputs);
        return *document.addFeature(std::move(draft));
    }

    BodyId bodyOf(const FeatureId& id, std::size_t index = 0) const {
        return document.findFeature(id)->outputs.at(index);
    }

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

TEST_CASE("split: a box in two at its middle", "[split][golden]") {
    World world;
    const FeatureId box =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const FeatureId split =
        world.add("split", {{"axis", "0"}, {"position", "10"}}, {world.bodyOf(box)});
    const RecomputeResult result = world.run();
    REQUIRE(result.state(split)->status == FeatureStatus::ok);

    const BodyResult& low = result.bodies.at(world.bodyOf(split, 0));
    const BodyResult& high = result.bodies.at(world.bodyOf(split, 1));
    const auto lowReport = world.kernel->measure(low.solid);
    const auto highReport = world.kernel->measure(high.solid);
    CHECK(lowReport.valid);
    CHECK(highReport.valid);
    CHECK(lowReport.volume == Approx(30.0 * 40.0 * 10.0).epsilon(1e-6));
    CHECK(highReport.volume == Approx(10.0 * 40.0 * 10.0).epsilon(1e-6));
    CHECK(lowReport.faces == 6);
    CHECK(highReport.faces == 6);
    CHECK(lowReport.boxMax.x == Approx(10.0));
    CHECK(highReport.boxMin.x == Approx(10.0));

    // The faces of the cut are named after the step, the others keep their names.
    const auto lowNames = namesOf(low);
    const auto highNames = namesOf(high);
    CHECK(lowNames.count("F2/split/cutA") == 1);
    CHECK(highNames.count("F2/split/cutB") == 1);
    CHECK(lowNames.count("F1/box/left") == 1);
    CHECK(lowNames.count("F1/box/right") == 0);
    CHECK(highNames.count("F1/box/right") == 1);
    CHECK(lowNames.count("F1/box/top") == 1);
    CHECK(highNames.count("F1/box/top") == 1);
}

TEST_CASE("split: along every axis", "[split][golden]") {
    for (const char* axis : {"0", "1", "2"}) {
        World world;
        const FeatureId box =
            world.add("box", {{"width", "20"}, {"depth", "20"}, {"height", "20"}, {"x", "0"}});
        // The middle of the box on each axis: 0, 0 and 10.
        const char* middle = std::string(axis) == "2" ? "10" : "0";
        const FeatureId split =
            world.add("split", {{"axis", axis}, {"position", middle}}, {world.bodyOf(box)});
        const RecomputeResult result = world.run();
        REQUIRE(result.state(split)->status == FeatureStatus::ok);
        CHECK(world.kernel->measure(result.bodies.at(world.bodyOf(split, 0)).solid).volume ==
              Approx(4000.0).epsilon(1e-6));
        CHECK(world.kernel->measure(result.bodies.at(world.bodyOf(split, 1)).solid).volume ==
              Approx(4000.0).epsilon(1e-6));
    }
}

TEST_CASE("split: a plane beside the body is an error that points at the position", "[split]") {
    World world;
    const FeatureId box = world.add("box", {{"x", "0"}});
    const FeatureId split =
        world.add("split", {{"axis", "0"}, {"position", "50"}}, {world.bodyOf(box)});
    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(split);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::kernelRejected);
    CHECK(state->issue->subject == "position");
}

TEST_CASE("split: a part can be moved on its own with a place step", "[split][place]") {
    World world;
    const FeatureId box =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const FeatureId split =
        world.add("split", {{"axis", "0"}, {"position", "0"}}, {world.bodyOf(box)});
    const FeatureId movedLow =
        world.add("place", {{"x", "-30"}, {"z", "5"}}, {world.bodyOf(split, 0)});
    const RecomputeResult result = world.run();
    REQUIRE(result.state(movedLow)->status == FeatureStatus::ok);

    const BodyResult& moved = result.bodies.at(world.bodyOf(movedLow));
    const auto report = world.kernel->measure(moved.solid);
    CHECK(report.volume == Approx(20.0 * 40.0 * 10.0).epsilon(1e-6));
    CHECK(report.boxMin.x == Approx(-50.0));
    CHECK(report.boxMin.z == Approx(5.0));
    // The names went along, the other part stayed.
    CHECK(namesOf(moved).count("F2/split/cutA") == 1);
    CHECK(world.kernel->measure(result.bodies.at(world.bodyOf(split, 1)).solid).boxMin.x ==
          Approx(0.0));
    // Both parts of the split are used by steps: only the moved body and the other part show.
    CHECK(world.document.consumerOf(world.bodyOf(split, 0)).has_value());
    CHECK_FALSE(world.document.consumerOf(world.bodyOf(split, 1)).has_value());
}
