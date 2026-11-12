// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pulling a sketch region straight out of its plane (AP-2.3a, "Ziehen"): caps and sides named
// after the curves of the region, holes, symmetric and negative pulls, other planes, and the
// requests that are refused.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

SketchCurve line(Point2d a, Point2d b) {
    return {SketchCurveKind::line, {a, b}};
}
SketchCurve circle(Point2d middle, double radius) {
    return {SketchCurveKind::circle, {middle, {middle.x + radius, middle.y}}};
}
std::vector<SketchCurve> rectangle(double x0, double y0, double x1, double y1) {
    return {line({x0, y0}, {x1, y0}), line({x1, y0}, {x1, y1}), line({x1, y1}, {x0, y1}),
            line({x0, y1}, {x0, y0})};
}
std::vector<SketchCurve> join(std::vector<SketchCurve> a, const std::vector<SketchCurve>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
std::set<std::string> rolesOf(const NamedSolid& named) {
    std::set<std::string> roles;
    for (const NamedFace& face : named.faces) {
        roles.insert(face.role);
    }
    return roles;
}

} // namespace

TEST_CASE("extrude: a rectangle pulled 30 up is a box with named faces", "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    const NamedSolid named =
        k->extrudeRegion(rectangle(0, 0, 60, 40), {30, 20}, {}, {30.0, ExtrudeExtent::distance});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(72000.0).epsilon(1e-9));
    CHECK(report.faces == 6);
    CHECK(report.boxMin.x == Approx(0.0).margin(1e-6));
    CHECK(report.boxMin.y == Approx(0.0).margin(1e-6));
    CHECK(report.boxMin.z == Approx(0.0).margin(1e-6));
    CHECK(report.boxMax.x == Approx(60.0).margin(1e-6));
    CHECK(report.boxMax.y == Approx(40.0).margin(1e-6));
    CHECK(report.boxMax.z == Approx(30.0).margin(1e-6));
    const std::set<std::string> roles = rolesOf(named);
    CHECK(roles.contains("capStart"));
    CHECK(roles.contains("capEnd"));
    int sides = 0;
    for (const std::string& role : roles) {
        if (role.starts_with("side[c")) {
            ++sides;
        }
    }
    CHECK(sides == 4);
    CHECK(roles.size() == 6); // every role different
}

TEST_CASE("extrude: a circle pulled up is a cylinder", "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    const NamedSolid named =
        k->extrudeRegion({circle({0, 0}, 10)}, {0, 0}, {}, {30.0, ExtrudeExtent::distance});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(kPi * 100.0 * 30.0).epsilon(1e-6));
    CHECK(report.faces == 3);
}

TEST_CASE("extrude: a rectangle with a circle inside pulls a ring or the disc",
          "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    const std::vector<SketchCurve> sketch = join(rectangle(0, 0, 60, 40), {circle({30, 20}, 5)});
    const NamedSolid ring = k->extrudeRegion(sketch, {5, 5}, {}, {30.0, ExtrudeExtent::distance});
    CHECK(k->measure(ring.solid).volume == Approx((2400.0 - kPi * 25.0) * 30.0).epsilon(1e-6));
    const NamedSolid disc = k->extrudeRegion(sketch, {30, 20}, {}, {30.0, ExtrudeExtent::distance});
    CHECK(k->measure(disc.solid).volume == Approx(kPi * 25.0 * 30.0).epsilon(1e-6));
}

TEST_CASE("extrude: symmetric pulls half of the distance to each side", "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    const NamedSolid named =
        k->extrudeRegion(rectangle(0, 0, 60, 40), {30, 20}, {}, {30.0, ExtrudeExtent::symmetric});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(72000.0).epsilon(1e-9));
    CHECK(report.boxMin.z == Approx(-15.0).margin(1e-6));
    CHECK(report.boxMax.z == Approx(15.0).margin(1e-6));
}

TEST_CASE("extrude: a negative distance pulls the other way", "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    const NamedSolid named =
        k->extrudeRegion(rectangle(0, 0, 60, 40), {30, 20}, {}, {-30.0, ExtrudeExtent::distance});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(72000.0).epsilon(1e-9));
    CHECK(report.boxMin.z == Approx(-30.0).margin(1e-6));
    CHECK(report.boxMax.z == Approx(0.0).margin(1e-6));
}

TEST_CASE("extrude: on a turned plane the pull follows the plane's normal", "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    // u = X, v = Z: the normal is (0, -1, 0), so a pull of 30 goes to -Y.
    const SketchFrame frame{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    const NamedSolid named =
        k->extrudeRegion(rectangle(0, 0, 60, 40), {30, 20}, frame, {30.0, ExtrudeExtent::distance});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.boxMin.x == Approx(0.0).margin(1e-6));
    CHECK(report.boxMax.x == Approx(60.0).margin(1e-6));
    CHECK(report.boxMin.y == Approx(-30.0).margin(1e-6));
    CHECK(report.boxMax.y == Approx(0.0).margin(1e-6));
    CHECK(report.boxMin.z == Approx(0.0).margin(1e-6));
    CHECK(report.boxMax.z == Approx(40.0).margin(1e-6));
}

TEST_CASE("extrude: a point outside every region and a zero distance are refused",
          "[geometry][extrude]") {
    const auto k = occt::makeKernel();
    const std::vector<SketchCurve> rect = rectangle(0, 0, 60, 40);
    try {
        (void)k->extrudeRegion(rect, {100, 100}, {}, {30.0, ExtrudeExtent::distance});
        FAIL("a point outside every region must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
    try {
        (void)k->extrudeRegion(rect, {30, 20}, {}, {0.0, ExtrudeExtent::distance});
        FAIL("a zero distance must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
}
