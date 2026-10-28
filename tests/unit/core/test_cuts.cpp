// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Holes cut without grouping (AP-1.7b, ADR-0011, rule 13): the cuts are combine steps that the
// program keeps up to date in the same step as the change that needed them.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <numbers>
#include <vector>

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
    const Feature& pinStep =
        *world.history.document().findFeature(world.history.document().findBody(pin)->createdBy);

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
    const Feature& pinStep =
        *world.history.document().findFeature(world.history.document().findBody(pin)->createdBy);
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
    const Feature& step =
        *world.history.document().findFeature(world.history.document().findBody(first)->createdBy);
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

TEST_CASE("cuts: the bodies a hole touches, and the ones taken off the label", "[cuts]") {
    World world;
    const BodyId left =
        world.add("box", {{"width", "20"}, {"depth", "20"}, {"height", "10"}, {"x", "-12"}});
    const BodyId right =
        world.add("box", {{"width", "20"}, {"depth", "20"}, {"height", "10"}, {"x", "12"}});
    const BodyId pin = world.add(
        "cylinder", {{"radius", "3"}, {"height", "20"}, {"x", "0"}, {"z", "-5"}, {"y", "0"}});
    world.asHole(pin);
    world.reconcile();
    REQUIRE(world.cutSteps() == 2);

    auto targets =
        cutTargetsOf(world.history.document(), world.run(), world.registry, *world.kernel, pin);
    REQUIRE(targets.size() == 2);
    CHECK(targets[0].root == left);
    CHECK(targets[0].cutNow);
    CHECK(targets[1].root == right);

    // The person takes the right box off the label: its cut goes, and stays away.
    world.history.execute(makeSetBodyCutExceptions(pin, {right}));
    CHECK(world.reconcile());
    CHECK(world.cutSteps() == 1);
    CHECK_FALSE(world.reconcile());
    targets =
        cutTargetsOf(world.history.document(), world.run(), world.registry, *world.kernel, pin);
    REQUIRE(targets.size() == 2); // it still touches the right box ...
    for (const CutTarget& target : targets) {
        CHECK(target.cutNow == (target.root == left)); // ... but does not cut it
    }

    // And back on the label.
    world.history.execute(makeSetBodyCutExceptions(pin, {}));
    CHECK(world.reconcile());
    CHECK(world.cutSteps() == 2);
}

TEST_CASE("cuts: a body that is no hole has no targets", "[cuts]") {
    World world;
    world.plate();
    const BodyId pin = world.pin("0");
    CHECK(cutTargetsOf(world.history.document(), world.run(), world.registry, *world.kernel, pin)
              .empty());
}

TEST_CASE("cuts: a hole goes all the way through, also when the body grows", "[cuts][through]") {
    World world;
    const BodyId plate = world.plate();
    // A pin much shorter than the plate, standing inside it.
    const BodyId pin = world.add("cylinder", {{"radius", "3"}, {"height", "4"}, {"z", "3"}});
    world.asHole(pin);
    world.reconcile();
    const Feature& plateStep =
        *world.history.document().findFeature(world.history.document().findBody(plate)->createdBy);
    const Feature& pinStep =
        *world.history.document().findFeature(world.history.document().findBody(pin)->createdBy);

    // Through is the default: the hole leaves the plate on both sides.
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(3, 10)).epsilon(1e-6));

    // Quader height 10 -> 20: the hole still goes all the way through (Ablauf D, step 4).
    world.history.execute(makeSetFeatureValue(plateStep.id, "height", "20"));
    CHECK(world.volumeOfShown() == Approx(40.0 * 40.0 * 20.0 - hole(3, 20)).epsilon(1e-6));

    // Switched off, the hole is as long as it is.
    world.history.execute(makeSetFeatureValue(pinStep.id, "through", "0"));
    CHECK(world.volumeOfShown() == Approx(40.0 * 40.0 * 20.0 - hole(3, 4)).epsilon(1e-6));
}

TEST_CASE("cuts: through follows a turned hole along its own axis", "[cuts][through]") {
    World world;
    world.plate(); // 40 x 40 x 10
    // A pin lying along x, 4 mm long, in the middle of the plate.
    const BodyId pin = world.add(
        "cylinder", {{"radius", "2"}, {"height", "4"}, {"x", "0"}, {"z", "3"}, {"rotY", "90"}});
    world.asHole(pin);
    world.reconcile();
    // It goes through the whole length of the plate in x: 40 mm of a bore of radius 2 (it only
    // reaches half the height of the plate: the bore lies at z = 5 ± 2).
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(2, 40)).epsilon(1e-6));
}

TEST_CASE("cuts: a sphere as a hole is as big as it is", "[cuts][through]") {
    World world;
    world.plate();
    const BodyId ball = world.add("sphere", {{"radius", "4"}, {"x", "0"}, {"z", "3"}});
    world.asHole(ball);
    world.reconcile();
    CHECK(world.cutSteps() == 1);
    const double volume = world.volumeOfShown();
    CHECK(volume < kPlate);
    CHECK(volume > kPlate - 4.0 / 3.0 * std::numbers::pi * 64.0 - 1e-6);
}

TEST_CASE("cuts: a hole that stands on a body cuts it, one floating above does not",
          "[cuts][through]") {
    World world;
    world.plate(); // 40 x 40 x 10, on the bed
    // Put down on the top face: it touches it and nothing more. It goes all the way through.
    const BodyId down = world.add("cylinder", {{"radius", "3"}, {"height", "4"}, {"z", "10"}});
    world.asHole(down);
    world.reconcile();
    CHECK(world.cutSteps() == 1);
    CHECK(world.volumeOfShown() == Approx(kPlate - hole(3, 10)).epsilon(1e-6));

    World other;
    other.plate();
    const BodyId floating = other.add("cylinder", {{"radius", "3"}, {"height", "4"}, {"z", "30"}});
    other.asHole(floating);
    other.reconcile();
    CHECK(other.cutSteps() == 0);

    // A ball needs to sink in: standing on the plate it does not cut.
    World ball;
    ball.plate();
    const BodyId sphere = ball.add("sphere", {{"radius", "4"}, {"z", "10"}});
    ball.asHole(sphere);
    ball.reconcile();
    CHECK(ball.cutSteps() == 0);
}

TEST_CASE("cuts: how long the check takes (reported, not a limit)", "[cuts][time]") {
    World world;
    world.plate();
    const BodyId pin = world.pin("0");
    world.asHole(pin);
    const auto start = std::chrono::steady_clock::now();
    world.reconcile(); // cold: everything is calculated
    const auto cold = std::chrono::steady_clock::now();
    world.reconcile(); // warm: nothing changed
    const auto warm = std::chrono::steady_clock::now();
    const auto ms = [](auto from, auto to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };
    WARN("reconcileCuts: cold " << ms(start, cold) << " ms, warm " << ms(cold, warm) << " ms");
    CHECK(ms(cold, warm) < 500.0);
}

TEST_CASE("cuts: how long the check takes with 10 bodies and 20 holes (reported)", "[cuts][time]") {
    // pflege.md P13: reconcileCuts runs in the UI thread; from about 50 ms (warm) it must move
    // to the background. A row of 10 plates, every plate pierced by two pins.
    World world;
    for (int plate = 0; plate < 10; ++plate) {
        const std::string x = std::to_string(plate * 50);
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", x}});
        for (int pin = 0; pin < 2; ++pin) {
            const std::string px = std::to_string(plate * 50 + (pin == 0 ? -8 : 8));
            world.asHole(world.pin(px.c_str()));
        }
    }
    const auto ms = [](auto from, auto to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };
    const auto start = std::chrono::steady_clock::now();
    world.reconcile(); // cold: everything is calculated
    const auto cold = std::chrono::steady_clock::now();
    world.reconcile(); // warm: nothing changed
    const auto warm = std::chrono::steady_clock::now();
    WARN("reconcileCuts, 10 bodies 20 holes: cold "
         << ms(start, cold) << " ms, warm " << ms(cold, warm) << " ms, cuts " << world.cutSteps());
    CHECK(world.cutSteps() == 20);
}

TEST_CASE("cuts: one step of a drag with 10 bodies and 20 holes (reported)", "[cuts][time]") {
    // pflege.md P13: what rule 9 asks for while a hole is pulled is the time of one step of the
    // drag (one hole moved, the rest unchanged), not that of all the cuts at once.
    World world;
    std::vector<BodyId> pins;
    for (int plate = 0; plate < 10; ++plate) {
        const std::string x = std::to_string(plate * 50);
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", x}});
        for (int pin = 0; pin < 2; ++pin) {
            const std::string px = std::to_string(plate * 50 + (pin == 0 ? -8 : 8));
            pins.push_back(world.pin(px.c_str()));
            world.asHole(pins.back());
        }
    }
    world.reconcile();
    world.reconcile();
    REQUIRE(world.cutSteps() == 20);
    const Feature& pinStep = *world.history.document().findFeature(
        world.history.document().findBody(pins.front())->createdBy);
    std::vector<double> steps;
    for (int i = 1; i <= 8; ++i) {
        // The history of a drag is open: every step is part of one undo step.
        world.history.execute(makeSetFeatureValue(pinStep.id, "x", std::to_string(-8.0 + 0.5 * i)));
        const auto start = std::chrono::steady_clock::now();
        world.reconcile();
        steps.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count());
    }
    double sum = 0.0;
    double worst = 0.0;
    for (double ms : steps) {
        sum += ms;
        worst = std::max(worst, ms);
    }
    WARN("reconcileCuts, one hole moved among 20: mean " << sum / steps.size() << " ms, worst "
                                                         << worst << " ms");
    CHECK(world.cutSteps() == 20);
}

TEST_CASE("cuts: a hole as wide as the body, a hair above it, still cuts", "[cuts][through]") {
    World world;
    world.add("box", {{"width", "20"}, {"depth", "20"}, {"height", "20"}, {"x", "0"}});
    // Radius 10 on a box 20 wide (the first try of everybody), off centre, 0.00001 mm above.
    const BodyId pin =
        world.add("cylinder",
                  {{"radius", "10"}, {"height", "20"}, {"x", "0"}, {"y", "-7"}, {"z", "20.00001"}});
    world.asHole(pin);
    world.reconcile();
    CHECK(world.cutSteps() == 1);
}
