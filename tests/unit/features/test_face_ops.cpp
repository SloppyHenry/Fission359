// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The steps "stretch", "removeFaces", "offsetFaces" and "tiltFace" (AP-2.4b-e): faces come from
// persistent references, the result keeps the names of the body.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;

namespace {

constexpr double kPi = std::numbers::pi;

std::string faceNamed(RefWorld& world, const core::RecomputeResult& result,
                      const core::FeatureId& step, const std::string& suffix) {
    for (const core::NamedLook& face : world.looksOf(result, step).faces) {
        if (face.name.size() > suffix.size() &&
            face.name.compare(face.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return face.name;
        }
    }
    FAIL("no face ending in " << suffix);
    return {};
}

// A 40 x 40 x 10 plate with a hole of radius 3 in the middle (a "combine" step).
core::FeatureId plateWithHole(RefWorld& world) {
    const core::FeatureId box = world.add(
        "box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}, {"y", "0"}});
    // The box is centred on x and y: it spans -20 to 20 both ways, the hole is in the middle.
    const core::FeatureId pin = world.add(
        "cylinder", {{"radius", "3"}, {"height", "30"}, {"x", "0"}, {"y", "0"}, {"z", "-10"}});
    return world.add("combine", {}, {world.bodyOf(box), world.bodyOf(pin)}, {{"mode", "subtract"}});
}

core::FeatureId faceStep(RefWorld& world, const char* type, const core::FeatureId& target,
                         const char* slot, std::vector<core::PersistentRef> refs,
                         std::map<std::string, std::string> values) {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find(type));
    draft.inputs = {world.bodyOf(target)};
    draft.refs[slot] = std::move(refs);
    for (const auto& [name, expression] : values) {
        draft.params.at(name).expression = expression;
    }
    return *world.document.addFeature(std::move(draft));
}

std::string why(const core::RecomputeResult& result, const core::FeatureId& step) {
    const core::FeatureState* state = result.state(step);
    return state != nullptr && state->issue
               ? "code " + std::to_string(int(state->issue->code)) + " subject " +
                     state->issue->subject + " detail " + state->issue->detail
               : "no problem";
}

double volumeOf(RefWorld& world, const core::RecomputeResult& result, const core::FeatureId& step) {
    const fission::geo::SolidReport report =
        world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid);
    CHECK(report.valid);
    return report.volume;
}

} // namespace

TEST_CASE("stretch: a plate with a hole gets longer and the hole stays", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = plateWithHole(world);
    const core::RecomputeResult first = world.run();
    const core::FeatureId step =
        faceStep(world, "stretch", plate, "face",
                 {world.pick(first, plate, core::ElementKind::face,
                             faceNamed(world, first, plate, "/box/right"))},
                 {{"distance", "10"}, {"position", "11.5"}});

    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    INFO(why(result, step));
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, step) ==
          Approx(50.0 * 40.0 * 10.0 - kPi * 9.0 * 10.0).epsilon(1e-6));
    bool hole = false;
    for (const core::BodyFace& face : result.bodies.at(world.bodyOf(step)).faces) {
        hole = hole || face.name == "F2/cylinder/side";
    }
    CHECK(hole); // the wall of the hole is still the same face

    // A cutting place through the hole is a problem that helps, not a wrong body.
    REQUIRE(world.document.setFeatureValue(step, "position", "0"));
    const core::RecomputeResult bad = world.run();
    CHECK(bad.state(step)->status == core::FeatureStatus::error);
    REQUIRE(bad.state(step)->issue.has_value());
    CHECK(bad.state(step)->issue->code == core::FeatureIssueCode::kernelRejected);
    CHECK(bad.state(step)->issue->subject == "position");
}

TEST_CASE("removeFaces: the wall of a hole is taken away and the plate is whole", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = plateWithHole(world);
    const core::RecomputeResult first = world.run();
    const core::FeatureId step =
        faceStep(world, "removeFaces", plate, "faces",
                 {world.pick(first, plate, core::ElementKind::face,
                             faceNamed(world, first, plate, "/cylinder/side"))},
                 {});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    INFO(why(result, step));
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, step) == Approx(40.0 * 40.0 * 10.0).epsilon(1e-9));
    CHECK(result.bodies.at(world.bodyOf(step)).faces.size() == 6);
}

TEST_CASE("offsetFaces: all faces moved in make a fit", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("40", "40", "10");
    const core::RecomputeResult first = world.run();
    std::vector<core::PersistentRef> all;
    for (const core::NamedLook& face : world.looksOf(first, plate).faces) {
        all.push_back(world.pick(first, plate, core::ElementKind::face, face.name));
    }
    REQUIRE(all.size() == 6);
    const core::FeatureId step =
        faceStep(world, "offsetFaces", plate, "faces", std::move(all), {{"distance", "-0.2"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    INFO(why(result, step));
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, step) == Approx(39.6 * 39.6 * 9.6).epsilon(1e-9));
}

TEST_CASE("tiltFace: a wall turned about its bottom edge cuts a wedge", "[faces]") {
    RefWorld world;
    const core::FeatureId plate = world.plate("20", "20", "20");
    const core::RecomputeResult first = world.run();
    const core::FeatureId step = faceStep(world, "tiltFace", plate, "face",
                                          {world.pick(first, plate, core::ElementKind::face,
                                                      faceNamed(world, first, plate, "/right"))},
                                          {{"angle", "-30"},
                                           {"pivotX", "10"},
                                           {"pivotY", "0"},
                                           {"pivotZ", "0"},
                                           {"axisX", "0"},
                                           {"axisY", "1"},
                                           {"axisZ", "0"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    INFO(why(result, step));
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, step) ==
          Approx(8000.0 - 0.5 * 20.0 * 20.0 * std::tan(30.0 * kPi / 180.0) * 20.0).epsilon(1e-9));

    // No angle is refused as a value that does not work.
    REQUIRE(world.document.setFeatureValue(step, "angle", "0"));
    CHECK(world.run().state(step)->status == core::FeatureStatus::error);
}
