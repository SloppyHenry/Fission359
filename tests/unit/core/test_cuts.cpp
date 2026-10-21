// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Holes cut without grouping (AP-1.7b, ADR-0011, rule 13): the cuts are combine steps that the
// program keeps up to date in the same step as the change that needed them.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

#include "fission/core/commands.h"
#include "fission/core/cuts.h"
#include "fission/core/history.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;

namespace {

struct World {
    History history;
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;

    World() { fission::features::registerBuiltinFeatures(registry); }

    BodyId add(const char* type, std::map<std::string, std::string> values = {}) {
        FeatureDraft draft = defaultDraft(*registry.find(type));
        for (const auto& [name, text] : values) {
            draft.params.at(name).expression = text;
        }
        const auto id = history.addFeature(std::move(draft));
        REQUIRE(id.has_value());
        return history.document().findFeature(*id)->outputs.front();
    }

    // A plate 40 x 40 x 10 and a pin of radius 3 (a hole once marked), standing on z = `z`.
    BodyId plate() {
        return add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    }
    BodyId pin(const char* x, const char* z = "-5") {
        return add("cylinder", {{"radius", "3"}, {"height", "20"}, {"x", x}, {"z", z}});
    }

    void asHole(const BodyId& body, bool hole = true) {
        history.execute(makeSetBodyHole(body, hole));
    }

    bool reconcile() { return reconcileCuts(history, registry, *kernel, cache); }

    RecomputeResult run() {
        auto result = recompute(history.document(), registry, *kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }

    // The volume of the one body nobody uses as an input and that is not a hole.
    double volumeOfShown() {
        const RecomputeResult result = run();
        double volume = -1.0;
        int shown = 0;
        for (const Body& body : history.document().bodies()) {
            if (!body.hole && !history.document().consumerOf(body.id)) {
                volume = kernel->measure(result.bodies.at(body.id).solid).volume;
                ++shown;
            }
        }
        REQUIRE(shown == 1);
        return volume;
    }

    std::size_t cutSteps() const {
        std::size_t count = 0;
        for (const Feature& step : history.document().features()) {
            count += step.type == "combine" ? 1 : 0;
        }
        return count;
    }
};

constexpr double kPlate = 40.0 * 40.0 * 10.0;

double hole(double radius, double depth) {
    return std::numbers::pi * radius * radius * depth;
}

} // namespace

TEST_CASE("cuts: nothing to do without holes", "[cuts]") {
    World world;
    world.plate();
    world.pin("0");
    CHECK_FALSE(world.reconcile());
    CHECK(world.cutSteps() == 0);
}

TEST_CASE("cuts: a hole that touches a body cuts it", "[cuts]") {
    World world;
    world.plate();
    const BodyId pin = world.pin("0");
    world.asHole(pin);
    REQUIRE(world.reconcile());

    CHECK(world.cutSteps() == 1);
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(3, 10)).epsilon(1e-6));
    // Asking again changes nothing.
    CHECK_FALSE(world.reconcile());
    CHECK(world.cutSteps() == 1);
}

TEST_CASE("cuts: a hole beside the body cuts nothing", "[cuts]") {
    World world;
    world.plate();
    const BodyId pin = world.pin("100");
    world.asHole(pin);
    CHECK_FALSE(world.reconcile());
    CHECK(world.cutSteps() == 0);
}

TEST_CASE("cuts: a hole that moves away is taken out, back in is cut again", "[cuts]") {
    World world;
    const BodyId plate = world.plate();
    const BodyId pin = world.pin("0");
    world.asHole(pin);
    world.reconcile();
    const Feature& pinStep = *world.history.document().findFeature(
        world.history.document().findBody(pin)->createdBy);

    world.history.execute(makeSetFeatureValue(pinStep.id, "x", "100"));
    CHECK(world.reconcile());
    CHECK(world.cutSteps() == 0);
    CHECK(world.volumeOfShown() == Approx(kPlate).epsilon(1e-6));
    // The plate is a body of its own again.
    CHECK_FALSE(world.history.document().consumerOf(plate).has_value());

    world.history.execute(makeSetFeatureValue(pinStep.id, "x", "10"));
    CHECK(world.reconcile());
    CHECK(world.cutSteps() == 1);
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(3, 10)).epsilon(1e-6));
}

TEST_CASE("cuts: moving a hole and its cuts are one step to undo", "[cuts]") {
    World world;
    world.plate();
    const BodyId pin = world.pin("100");
    world.asHole(pin);
    const Feature& pinStep = *world.history.document().findFeature(
        world.history.document().findBody(pin)->createdBy);
    const std::size_t before = world.history.position();

    world.history.beginTransaction({"moveBody", {}});
    world.history.execute(makeSetFeatureValue(pinStep.id, "x", "0"));
    world.reconcile();
    world.history.commitTransaction();
    CHECK(world.history.position() == before + 1);
    CHECK(world.cutSteps() == 1);

    REQUIRE(world.history.undo());
    CHECK(world.cutSteps() == 0);
    CHECK(world.volumeOfShown() == Approx(kPlate).epsilon(1e-6));
    REQUIRE(world.history.redo());
    CHECK(world.cutSteps() == 1);
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(3, 10)).epsilon(1e-6));
}

TEST_CASE("cuts: taking the mark away gives the body back", "[cuts]") {
    World world;
    world.plate();
    const BodyId pin = world.pin("0");
    world.asHole(pin);
    world.reconcile();

    world.asHole(pin, false);
    CHECK(world.reconcile());
    CHECK(world.cutSteps() == 0);
    // Pin and plate are two bodies again.
    int shown = 0;
    for (const Body& body : world.history.document().bodies()) {
        shown += world.history.document().consumerOf(body.id) ? 0 : 1;
    }
    CHECK(shown == 2);
}

TEST_CASE("cuts: two holes make two cuts that follow one another", "[cuts]") {
    World world;
    world.plate();
    const BodyId first = world.pin("-10");
    const BodyId second = world.pin("10");
    world.asHole(first);
    world.asHole(second);
    world.reconcile();

    CHECK(world.cutSteps() == 2);
    CHECK(world.volumeOfShown() == Approx(kPlate - 2.0 * hole(3, 10)).epsilon(1e-6));

    // Taking the first out leaves the second cutting the plate.
    const Feature& step = *world.history.document().findFeature(
        world.history.document().findBody(first)->createdBy);
    world.history.execute(makeSetFeatureValue(step.id, "x", "100"));
    world.reconcile();
    CHECK(world.cutSteps() == 1);
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(3, 10)).epsilon(1e-6));
}

TEST_CASE("cuts: one hole through two bodies cuts both", "[cuts]") {
    World world;
    world.add("box", {{"width", "20"}, {"depth", "20"}, {"height", "10"}, {"x", "-12"}});
    world.add("box", {{"width", "20"}, {"depth", "20"}, {"height", "10"}, {"x", "12"}});
    const BodyId pin = world.add(
        "cylinder", {{"radius", "3"}, {"height", "20"}, {"x", "0"}, {"z", "-5"}, {"y", "0"}});
    world.asHole(pin);
    world.reconcile();
    CHECK(world.cutSteps() == 2);
}

TEST_CASE("cuts: steps made by hand are left alone", "[cuts]") {
    World world;
    const BodyId plate = world.plate();
    const BodyId pin = world.pin("0");
    FeatureDraft draft = defaultDraft(*world.registry.find("combine"));
    draft.texts["mode"] = "subtract"; // auto stays "no"
    draft.inputs = {plate, pin};
    REQUIRE(world.history.addFeature(std::move(draft)).has_value());
    CHECK_FALSE(world.reconcile());
    CHECK(world.cutSteps() == 1);
}
