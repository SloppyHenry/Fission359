// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Kernel robustness catalog, group A: unions and cuts with the situations makers run into.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>
#include <vector>

#include "support.h"

using Catch::Approx;
using namespace fission::test::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// A goal volume with a tolerance of 1e-6 (relative), as required for golden tests.
Approx volumeIs(double expected) {
    return Approx(expected).epsilon(1e-6);
}

} // namespace

TEST_CASE("A1 union of two boxes that share a whole face", "[geometry][boolean]") {
    const BoolResult result = fuse(makeBox(20, 20, 10), makeBox(20, 20, 10, 0, 0, 10));
    REQUIRE(result.done);
    const ShapeStats raw = measure(result.shape);
    const ShapeStats merged = measure(unifyFaces(result.shape));

    note("faces before merge", raw.faces);
    note("faces after merge", merged.faces);
    CHECK_FALSE(result.hasErrors);
    CHECK(raw.valid);
    CHECK(raw.volume == volumeIs(8000.0));
    CHECK(raw.solids == 1);
    // The user sees one box: after merging the same-surface faces there are six.
    CHECK(merged.valid);
    CHECK(merged.faces == 6);
}

TEST_CASE("A2 round hole through a plate", "[geometry][boolean]") {
    const BoolResult result = cut(makeBox(40, 40, 10), makeCylinder(5, 12, 20, 20, -1));
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    CHECK(stats.valid);
    CHECK(stats.volume == volumeIs(16000.0 - kPi * 25.0 * 10.0));
    CHECK(stats.faces == 7);
    CHECK(stats.solids == 1);
}

TEST_CASE("A3 hole that just touches the outer wall", "[geometry][boolean]") {
    // The hole wall is tangent to the side face x = 0: a zero-thickness web.
    const BoolResult result = cut(makeBox(40, 40, 10), makeCylinder(5, 12, 5, 20, -1));
    note("done", result.done ? "yes" : "no");
    note("errors", result.hasErrors ? "yes" : "no");
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    note("faces", stats.faces);
    note("valid", stats.valid ? "yes" : "no");
    CHECK(stats.valid);
    CHECK(stats.volume == volumeIs(16000.0 - kPi * 25.0 * 10.0));
}

TEST_CASE("A4 union of two coaxial cylinders with the same radius", "[geometry][boolean]") {
    const BoolResult result = fuse(makeCylinder(10, 20), makeCylinder(10, 10, 0, 0, 20));
    REQUIRE(result.done);
    const ShapeStats raw = measure(result.shape);
    const ShapeStats merged = measure(unifyFaces(result.shape));

    note("faces before merge", raw.faces);
    note("faces after merge", merged.faces);
    CHECK(raw.valid);
    CHECK(raw.volume == volumeIs(kPi * 100.0 * 30.0));
    CHECK(merged.faces == 3); // top, bottom, one side
}

TEST_CASE("A5 cut of a coaxial cylinder with the same radius", "[geometry][boolean]") {
    const BoolResult result = cut(makeCylinder(10, 20), makeCylinder(10, 10));
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    CHECK(stats.valid);
    CHECK(stats.volume == volumeIs(kPi * 100.0 * 10.0));
    CHECK(stats.solids == 1);
}

TEST_CASE("A6 grid of 100 holes in one cut", "[geometry][boolean]") {
    std::vector<TopoDS_Shape> holes;
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 10; ++column) {
            holes.push_back(makeCylinder(3, 7, 5.0 + 10.0 * column, 5.0 + 10.0 * row, -1.0));
        }
    }
    const BoolResult result = cutMany(makeBox(100, 100, 5), holes);
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    note("seconds", result.seconds);
    note("faces", stats.faces);
    CHECK(stats.valid);
    CHECK(stats.volume == volumeIs(50000.0 - 100.0 * kPi * 9.0 * 5.0));
    CHECK(stats.faces == 6 + 100);
}

TEST_CASE("A7 two boxes that touch only along an edge", "[geometry][boolean]") {
    const BoolResult result = fuse(makeBox(10, 10, 10), makeBox(10, 10, 10, 10, 10, 0));
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    note("solids", stats.solids);
    CHECK(stats.valid);
    CHECK(stats.volume == volumeIs(2000.0));
}

TEST_CASE("A8 tiny hole (0.05 mm) in a large plate (200 mm)", "[geometry][boolean]") {
    const BoolResult result = cut(makeBox(200, 200, 10), makeBox(0.05, 0.05, 12, 100, 100, -1));
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    CHECK(stats.valid);
    CHECK(stats.volume == Approx(400000.0 - 0.05 * 0.05 * 10.0).epsilon(1e-9));
    CHECK(stats.faces == 10);
}

TEST_CASE("A9 boxes that overlap by only 0.00001 mm", "[geometry][boolean]") {
    // Typical result of a typed-in value or a rounding error: nearly, but not exactly, flush.
    const BoolResult result = fuse(makeBox(10, 10, 10), makeBox(10, 10, 10, 0, 0, 9.99999));
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    note("faces", stats.faces);
    CHECK(stats.valid);
    CHECK(stats.volume == volumeIs(2000.0 - 10.0 * 10.0 * 0.00001));
    CHECK(stats.solids == 1);
}

// Fallbacks and one more everyday situation ----------------------------------------------

TEST_CASE("A9a nearly flush boxes united with a fuzzy value", "[geometry][boolean]") {
    // Same input as A9. The fuzzy value tells the kernel to treat distances below 0.001 mm as
    // zero, which removes the sliver faces.
    const BoolResult result =
        fuse(makeBox(10, 10, 10), makeBox(10, 10, 10, 0, 0, 9.99999), /*fuzzy=*/0.001);
    REQUIRE(result.done);
    const ShapeStats stats = measure(unifyFaces(result.shape));

    note("faces", stats.faces);
    note("volume", stats.volume);
    CHECK(stats.valid);
    CHECK(stats.faces == 6);
    CHECK(stats.volume == volumeIs(2000.0 - 10.0 * 10.0 * 0.00001));
}

TEST_CASE("A10 parts separated by 0.001 mm are not merged", "[geometry][boolean]") {
    // Two boxes that look touching but have a 1 micron gap (a typed 10.001 instead of 10).
    const TopoDS_Shape a = makeBox(10, 10, 10);
    const TopoDS_Shape b = makeBox(10, 10, 10, 10.001, 0, 0);
    const BoolResult plain = fuse(a, b);
    const BoolResult fuzzy = fuse(a, b, /*fuzzy=*/0.01);
    REQUIRE(plain.done);
    REQUIRE(fuzzy.done);
    const ShapeStats exact = measure(plain.shape);
    const ShapeStats healed = measure(fuzzy.shape);

    note("solids exact", exact.solids);
    note("solids fuzzy 0.01", healed.solids);
    note("faces exact", exact.faces);
    note("faces fuzzy 0.01", healed.faces);
    CHECK(exact.valid);
    CHECK(exact.solids == 2); // honest: they are two parts
    CHECK(healed.valid);
}
