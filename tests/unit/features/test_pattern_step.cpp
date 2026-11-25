// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The step "pattern" (AP-2.8b): copies of a body in a row or a circle, merged with the original.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <set>

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;
namespace geo = fission::geo;

namespace {

core::FeatureId addPattern(RefWorld& world, const core::FeatureId& box, const char* mode,
                           std::map<std::string, std::string> values, const char* skip = "") {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find("pattern"));
    draft.inputs = {world.bodyOf(box)};
    draft.texts["mode"] = mode;
    draft.texts["skip"] = skip;
    for (const auto& [name, expression] : values) {
        draft.params.at(name).expression = expression;
    }
    return *world.document.addFeature(std::move(draft));
}

double volumeOf(RefWorld& world, const core::RecomputeResult& result, const core::FeatureId& step) {
    return world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid).volume;
}

geo::SolidReport reportOf(RefWorld& world, const core::RecomputeResult& result,
                          const core::FeatureId& step) {
    return world.kernel->measure(result.bodies.at(world.bodyOf(step)).solid);
}

std::vector<std::string> namesOf(RefWorld& world, const core::RecomputeResult& result,
                                 const core::FeatureId& step) {
    std::vector<std::string> names;
    for (const core::BodyFace& face : result.bodies.at(world.bodyOf(step)).faces) {
        names.push_back(face.name);
    }
    return names;
}

bool hasIndex(const std::vector<std::string>& names, int index) {
    const std::string tag = "[" + std::to_string(index) + "]";
    return std::any_of(names.begin(), names.end(), [&](const std::string& name) {
        return name.find(tag) != std::string::npos;
    });
}

// A 10 mm cube 30 mm from the axis (the z axis), for circular patterns.
core::FeatureId offAxisCube(RefWorld& world) {
    return world.add("box", {{"width", "10"}, {"depth", "10"}, {"height", "10"}, {"x", "30"}});
}

} // namespace

TEST_CASE("pattern: a row has separate parts, valid, with unique names", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "3"}, {"spacing", "2 cm"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(row)->status == core::FeatureStatus::ok);
    const geo::SolidReport report = reportOf(world, result, row);
    CHECK(report.valid);
    CHECK(report.solids == 3);
    CHECK(report.faces == 18);
    CHECK(report.volume == Approx(3000.0).epsilon(1e-9));
    const std::vector<std::string> names = namesOf(world, result, row);
    CHECK(std::set<std::string>(names.begin(), names.end()).size() == names.size());
    CHECK(hasIndex(names, 1));
    CHECK(hasIndex(names, 2));
}

TEST_CASE("pattern: a circle of four at 360 degrees has no copy on the original",
          "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId cube = offAxisCube(world);
    const core::FeatureId ring = addPattern(world, cube, "circular", {{"count", "4"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(ring)->status == core::FeatureStatus::ok);
    const geo::SolidReport report = reportOf(world, result, ring);
    CHECK(report.valid);
    CHECK(report.solids == 4);
    CHECK(report.volume == Approx(4000.0).epsilon(1e-9));
}

TEST_CASE("pattern: a circle over part of the turn spreads copies up to the end angle",
          "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId cube = offAxisCube(world);
    const core::FeatureId arc =
        addPattern(world, cube, "circular", {{"count", "3"}, {"total", "90"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(arc)->status == core::FeatureStatus::ok);
    const geo::SolidReport report = reportOf(world, result, arc);
    CHECK(report.valid);
    CHECK(report.solids == 3);
    // The last copy is a quarter turn away: the box reaches into y of the first one's x.
    CHECK(report.boxMax.y > 25.0);
}

TEST_CASE("pattern: a circle about a tilted axis turns about that axis", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId cube = offAxisCube(world);
    const core::FeatureId ring =
        addPattern(world, cube, "circular",
                   {{"count", "2"}, {"axisDx", "1"}, {"axisDy", "0"}, {"axisDz", "0"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(ring)->status == core::FeatureStatus::ok);
    // About the x axis a half turn sends z to -z: the copy lies below the plane z = 0.
    CHECK(reportOf(world, result, ring).boxMin.z < -5.0);
}

TEST_CASE("pattern: left-out copies keep the numbers of the others", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "4"}, {"spacing", "20"}}, " 1 , 3");
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(reportOf(world, result, row).solids == 2);
    const std::vector<std::string> names = namesOf(world, result, row);
    CHECK(hasIndex(names, 2));
    CHECK_FALSE(hasIndex(names, 1));
    CHECK_FALSE(hasIndex(names, 3));
}

TEST_CASE("pattern: the original cannot be left out; an index beyond the count is harmless",
          "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "2"}, {"spacing", "20"}}, "0, 9");
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(reportOf(world, result, row).solids == 2);
}

TEST_CASE("pattern: a skip list that is not whole numbers is a problem, not ignored",
          "[pattern][feature]") {
    for (const char* bad : {"x", "1,,2", "-1", "1.5", "1;2"}) {
        RefWorld world;
        const core::FeatureId box = world.plate("10", "10", "10");
        const core::FeatureId row =
            addPattern(world, box, "linear", {{"count", "3"}, {"spacing", "20"}}, bad);
        const core::RecomputeResult result = world.run();
        INFO("skip = " << bad);
        REQUIRE(result.state(row) != nullptr);
        CHECK(result.state(row)->status == core::FeatureStatus::error);
    }
}

TEST_CASE("pattern: changing the original changes every copy", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "3"}, {"spacing", "20"}});
    REQUIRE(world.document.setFeatureValue(box, "height", "15"));
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, row) == Approx(4500.0).epsilon(1e-9));
}

TEST_CASE("pattern: a hundred copies are quick enough", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "100"}, {"spacing", "20"}});
    const auto start = std::chrono::steady_clock::now();
    const core::RecomputeResult result = world.run();
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    REQUIRE(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(reportOf(world, result, row).solids == 100);
    CHECK(volumeOf(world, result, row) == Approx(100000.0).epsilon(1e-9));
    WARN("100 copies took " << seconds << " s");
#if defined(__EMSCRIPTEN__)
    // WebAssembly under Node, several test programs in parallel: the native budget does not apply.
    CHECK(seconds < 30.0);
#elif defined(NDEBUG)
    CHECK(seconds < 3.0);
#else
    CHECK(seconds < 30.0);
#endif
}

TEST_CASE("pattern: a row of three, with one left out", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId row =
        addPattern(world, box, "linear", {{"count", "3"}, {"spacing", "20"}});
    core::RecomputeResult result = world.run();
    REQUIRE(result.state(row) != nullptr);
    CHECK(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, row) == Approx(3000.0).epsilon(1e-9));

    REQUIRE(world.document.setFeatureText(row, "skip", "1"));
    result = world.run();
    CHECK(result.state(row)->status == core::FeatureStatus::ok);
    CHECK(volumeOf(world, result, row) == Approx(2000.0).epsilon(1e-9));
}

TEST_CASE("pattern: a wrong count or mode is a problem that helps", "[pattern][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("10", "10", "10");
    const core::FeatureId none = addPattern(world, box, "linear", {{"count", "0"}});
    const core::FeatureId odd = addPattern(world, box, "wobble", {});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(none) != nullptr);
    CHECK(result.state(none)->status == core::FeatureStatus::error);
    REQUIRE(result.state(odd) != nullptr);
    CHECK(result.state(odd)->status != core::FeatureStatus::ok);
}
