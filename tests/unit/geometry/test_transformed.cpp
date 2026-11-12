// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Transforming a body with a Transform3 (AP-2.8b): translation, rotation, mirror, and validation.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/pattern.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

} // namespace

TEST_CASE("transformed: a box translated by (20,0,0) has BBox min.x at 20 and volume 1000",
          "[geometry][pattern]") {
    const auto k = occt::makeKernel();
    const NamedSolid box = k->makeBox(10.0, 10.0, 10.0, {0, 0, 0}, 0.0);

    Transform3 trans;
    trans.t = {20.0, 0.0, 0.0}; // Pure translation

    const NamedSolid transformed = k->transformed(box, trans);
    const SolidReport report = k->measure(transformed.solid);

    CHECK(report.valid);
    CHECK(report.volume == Approx(1000.0).epsilon(1e-9));
    CHECK(report.boxMin.x == Approx(20.0).margin(1e-6));
    CHECK(report.boxMin.y == Approx(0.0).margin(1e-6));
    CHECK(report.boxMin.z == Approx(0.0).margin(1e-6));
    CHECK(report.boxMax.x == Approx(30.0).margin(1e-6));
}

TEST_CASE("transformed: a box rotated 90 degrees around z has volume 1000 and is valid",
          "[geometry][pattern]") {
    const auto k = occt::makeKernel();
    const NamedSolid box = k->makeBox(10.0, 10.0, 10.0, {0, 0, 0}, 0.0);

    // Rotation matrix for 90 degrees around z-axis (right-handed)
    // R = [cosθ, -sinθ, 0; sinθ, cosθ, 0; 0, 0, 1]
    const double cos90 = std::cos(kPi / 2.0);
    const double sin90 = std::sin(kPi / 2.0);

    Transform3 trans;
    trans.r[0][0] = cos90;
    trans.r[0][1] = -sin90;
    trans.r[0][2] = 0.0;
    trans.r[1][0] = sin90;
    trans.r[1][1] = cos90;
    trans.r[1][2] = 0.0;
    trans.r[2][0] = 0.0;
    trans.r[2][1] = 0.0;
    trans.r[2][2] = 1.0;

    const NamedSolid transformed = k->transformed(box, trans);
    const SolidReport report = k->measure(transformed.solid);

    CHECK(report.valid);
    CHECK(report.volume == Approx(1000.0).epsilon(1e-9));
}

TEST_CASE("transformed: mirror plane at x=5 through (5,0,0) with normal (1,0,0) gives valid result "
          "with volume 1000",
          "[geometry][pattern]") {
    const auto k = occt::makeKernel();
    const NamedSolid box = k->makeBox(10.0, 10.0, 10.0, {0, 0, 0}, 0.0);

    // Create mirror transformation for plane x=5, normal (1,0,0)
    const Transform3 mirror = mirrorPlane({5.0, 0.0, 0.0}, {1.0, 0.0, 0.0});

    const NamedSolid transformed = k->transformed(box, mirror);
    const SolidReport report = k->measure(transformed.solid);

    CHECK(report.valid);
    CHECK(report.volume == Approx(1000.0).epsilon(1e-9));
}

TEST_CASE("transformed: non-finite values in transform throw invalidInput", "[geometry][pattern]") {
    const auto k = occt::makeKernel();
    const NamedSolid box = k->makeBox(10.0, 10.0, 10.0, {0, 0, 0}, 0.0);

    Transform3 trans;
    trans.r[0][0] = std::numeric_limits<double>::infinity();

    try {
        (void)k->transformed(box, trans);
        FAIL("non-finite transform must throw");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
}

TEST_CASE("transformed: NaN values in transform throw invalidInput", "[geometry][pattern]") {
    const auto k = occt::makeKernel();
    const NamedSolid box = k->makeBox(10.0, 10.0, 10.0, {0, 0, 0}, 0.0);

    Transform3 trans;
    trans.t.x = std::numeric_limits<double>::quiet_NaN();

    try {
        (void)k->transformed(box, trans);
        FAIL("NaN transform must throw");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
}
