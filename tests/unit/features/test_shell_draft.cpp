// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The steps "shell" and "draft" (AP-2.6): faces come from persistent references, the result keeps
// the names of the body, a wall that is too thick is a problem that helps (with the numbers).

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

double footprint(double width, double depth, double radius) {
    return width * depth - (4.0 - kPi) * radius * radius;
}

core::FeatureId stepOn(RefWorld& world, const char* type, const core::FeatureId& target,
                       const char* slot, std::vector<core::PersistentRef> refs,
                       std::map<std::string, std::string> values,
                       std::map<std::string, std::string> words = {}) {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find(type));
    draft.inputs = {world.bodyOf(target)};
    if (!refs.empty()) {
        draft.refs[slot] = std::move(refs);
    }
    for (const auto& [name, expression] : values) {
        draft.params.at(name).expression = expression;
    }
    for (const auto& [name, text] : words) {
        draft.texts[name] = text;
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

} // namespace

TEST_CASE("shell: the rounded box with an open top, the classic case, from a step", "[hollow]") {
    RefWorld world;
    const core::FeatureId box = world.add("box", {{"width", "40"},
                                                  {"depth", "40"},
                                                  {"height", "20"},
                                                  {"x", "0"},
                                                  {"y", "0"},
                                                  {"cornerRadius", "5"}});
    const core::RecomputeResult first = world.run();
    const core::FeatureId step = stepOn(
        world, "shell", box, "openFaces",
        {world.pick(first, box, core::ElementKind::face, faceNamed(world, first, box, "/top"))},
        {{"thickness", "2"}});

    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    INFO(why(result, step));
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    const core::BodyResult& body = result.bodies.at(world.bodyOf(step));
    const fission::geo::SolidReport report = world.kernel->measure(body.solid);
    CHECK(report.valid);
    CHECK(report.shells == 1);
    CHECK(report.volume ==
          Approx(footprint(40, 40, 5) * 20.0 - footprint(36, 36, 3) * 18.0).epsilon(1e-6));
    bool bottom = false;
    bool inner = false;
    for (const core::BodyFace& face : body.faces) {
        bottom = bottom || face.name == "F1/box/bottom";
        inner = inner || face.name == "F2/shell/inner[bottom]";
    }
    CHECK(bottom);
    CHECK(inner);

    // The wall grows with the value; a wall larger than the radius makes sharp inner corners.
    REQUIRE(world.document.setFeatureValue(step, "thickness", "6"));
    const core::RecomputeResult thick = world.run();
    CHECK(thick.state(step)->status == core::FeatureStatus::ok);
    CHECK(world.kernel->measure(thick.bodies.at(world.bodyOf(step)).solid).volume ==
          Approx(footprint(40, 40, 5) * 20.0 - 28.0 * 28.0 * 14.0).epsilon(1e-6));
}

TEST_CASE("shell: a wall that is too thick is a problem with numbers", "[hollow]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "30", "40");
    const core::RecomputeResult first = world.run();
    const core::FeatureId step = stepOn(
        world, "shell", box, "openFaces",
        {world.pick(first, box, core::ElementKind::face, faceNamed(world, first, box, "/top"))},
        {{"thickness", "10"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    CHECK(result.state(step)->status == core::FeatureStatus::error);
    REQUIRE(result.state(step)->issue.has_value());
    CHECK(result.state(step)->issue->code == core::FeatureIssueCode::kernelRejected);
    CHECK(result.state(step)->issue->subject == "thickness");
    CHECK(result.state(step)->issue->numbers.at("thinnest") == Approx(20.0));
    CHECK(result.state(step)->issue->numbers.at("maxValue") == Approx(9.9));

    // No wall is refused as a value that does not work.
    REQUIRE(world.document.setFeatureValue(step, "thickness", "0"));
    CHECK(world.run().state(step)->status == core::FeatureStatus::error);
}

TEST_CASE("shell: the wall can be put around the body, and the step needs no open face",
          "[hollow]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "20", "20");
    const core::FeatureId closed =
        stepOn(world, "shell", box, "openFaces", {}, {{"thickness", "2"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(closed) != nullptr);
    INFO(why(result, closed));
    CHECK(result.state(closed)->status == core::FeatureStatus::ok);
    const fission::geo::SolidReport report =
        world.kernel->measure(result.bodies.at(world.bodyOf(closed)).solid);
    CHECK(report.shells == 2);
    CHECK(report.volume == Approx(8000.0 - 16.0 * 16.0 * 16.0).epsilon(1e-9));
}

TEST_CASE("draft: the sides of a box tilted by 5 degrees keep their names", "[draft]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "20", "10");
    const core::RecomputeResult first = world.run();
    std::vector<core::PersistentRef> sides;
    for (const char* role : {"/left", "/right", "/front", "/back"}) {
        sides.push_back(
            world.pick(first, box, core::ElementKind::face, faceNamed(world, first, box, role)));
    }
    // The box is centred on x and y and stands on z = 0: the neutral plane is its bottom.
    const core::FeatureId step = stepOn(world, "draft", box, "faces", std::move(sides),
                                        {{"angle", "5"},
                                         {"neutralX", "0"},
                                         {"neutralY", "0"},
                                         {"neutralZ", "0"},
                                         {"pullX", "0"},
                                         {"pullY", "0"},
                                         {"pullZ", "1"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step) != nullptr);
    INFO(why(result, step));
    CHECK(result.state(step)->status == core::FeatureStatus::ok);
    const double top = 20.0 - 2.0 * 10.0 * std::tan(5.0 * kPi / 180.0);
    CHECK(world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid).volume ==
          Approx(10.0 / 3.0 * (400.0 + top * top + 20.0 * top)).epsilon(1e-6));
    bool left = false;
    for (const core::BodyFace& face : result.bodies.at(world.bodyOf(step)).faces) {
        left = left || face.name == "F1/box/left";
    }
    CHECK(left);

    REQUIRE(world.document.setFeatureValue(step, "angle", "0"));
    CHECK(world.run().state(step)->status == core::FeatureStatus::error);
}
