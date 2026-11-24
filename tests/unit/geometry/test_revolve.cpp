// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Turning a sketch region about an axis in its plane (AP-2.3c, "Drehkörper"): a full turn makes a
// tube or a full cylinder, a half turn has two caps, a half disc makes a sphere, and the requests
// that are refused.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
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
SketchCurve arc(Point2d middle, double radius, double from, double to) {
    const double a = from * kPi / 180.0;
    const double b = to * kPi / 180.0;
    return {SketchCurveKind::arc,
            {middle,
             {middle.x + radius * std::cos(a), middle.y + radius * std::sin(a)},
             {middle.x + radius * std::cos(b), middle.y + radius * std::sin(b)}}};
}
std::vector<SketchCurve> rectangle(double x0, double y0, double x1, double y1) {
    return {line({x0, y0}, {x1, y0}), line({x1, y0}, {x1, y1}), line({x1, y1}, {x0, y1}),
            line({x0, y1}, {x0, y0})};
}
std::set<std::string> rolesOf(const NamedSolid& named) {
    std::set<std::string> roles;
    for (const NamedFace& face : named.faces) {
        roles.insert(face.role);
    }
    return roles;
}

} // namespace

TEST_CASE("revolve: a rectangle around the v-axis makes a tube", "[geometry][revolve]") {
    const auto k = occt::makeKernel();
    // u 10..20, v 0..30 around the v-axis: a tube of wall 10 and height 30.
    const NamedSolid named =
        k->revolveRegion(rectangle(10, 0, 20, 30), {15, 15}, {}, {{0, 0}, {0, 1}, 360.0});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(kPi * (20.0 * 20.0 - 10.0 * 10.0) * 30.0).epsilon(1e-6));
    CHECK(report.faces == 4);
    const std::set<std::string> roles = rolesOf(named);
    CHECK(!roles.contains("capStart"));
    CHECK(!roles.contains("capEnd"));
    int sides = 0;
    for (const std::string& role : roles) {
        if (role.starts_with("side[c")) {
            ++sides;
        }
    }
    CHECK(sides == 4);
    CHECK(roles.size() == 4); // every role different
}

TEST_CASE("revolve: a half turn has two caps and half the volume", "[geometry][revolve]") {
    const auto k = occt::makeKernel();
    const NamedSolid named =
        k->revolveRegion(rectangle(10, 0, 20, 30), {15, 15}, {}, {{0, 0}, {0, 1}, 180.0});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(kPi * (400.0 - 100.0) * 30.0 / 2.0).epsilon(1e-6));
    CHECK(report.faces == 6);
    const std::set<std::string> roles = rolesOf(named);
    CHECK(roles.contains("capStart"));
    CHECK(roles.contains("capEnd"));
    CHECK(roles.size() == 6); // every role different
}

TEST_CASE("revolve: a rectangle on the axis makes a full cylinder", "[geometry][revolve]") {
    const auto k = occt::makeKernel();
    // u 0..10 touches the axis with one edge: a cylinder of radius 10, no face on the axis.
    const NamedSolid named =
        k->revolveRegion(rectangle(0, 0, 10, 30), {5, 15}, {}, {{0, 0}, {0, 1}, 360.0});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(kPi * 100.0 * 30.0).epsilon(1e-6));
    CHECK(report.faces == 3);
    const std::set<std::string> roles = rolesOf(named);
    CHECK(roles.size() == 3); // every role different
}

TEST_CASE("revolve: a half disc about its diameter makes a sphere", "[geometry][revolve]") {
    const auto k = occt::makeKernel();
    const std::vector<SketchCurve> halfDisc = {arc({0, 0}, 10, 0, 180), line({-10, 0}, {10, 0})};
    const NamedSolid named = k->revolveRegion(halfDisc, {0, 5}, {}, {{0, 0}, {1, 0}, 360.0});
    const SolidReport report = k->measure(named.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(4.0 / 3.0 * kPi * 1000.0).epsilon(1e-6));
    CHECK(report.faces == 1);
    CHECK(rolesOf(named).contains("side[c0]"));
}

TEST_CASE("revolve: an area that crosses the axis, a zero axis and a bad angle are refused",
          "[geometry][revolve]") {
    const auto k = occt::makeKernel();
    const std::vector<SketchCurve> crossing = rectangle(-5, 0, 5, 30);
    try {
        (void)k->revolveRegion(crossing, {0, 15}, {}, {{0, 0}, {0, 1}, 360.0});
        FAIL("an area that crosses the axis must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "axis");
    }
    const std::vector<SketchCurve> rect = rectangle(10, 0, 20, 30);
    try {
        (void)k->revolveRegion(rect, {15, 15}, {}, {{0, 0}, {0, 0}, 360.0});
        FAIL("a zero axis direction must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
    try {
        (void)k->revolveRegion(rect, {15, 15}, {}, {{0, 0}, {0, 1}, 0.0});
        FAIL("a zero angle must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
    try {
        (void)k->revolveRegion(rect, {15, 15}, {}, {{0, 0}, {0, 1}, 400.0});
        FAIL("an angle of more than 360 degrees must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
    }
}
