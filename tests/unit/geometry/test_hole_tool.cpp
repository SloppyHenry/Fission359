// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The cutting tool of a drilled hole (AP-2.7b): volume against the formula, the face roles of
// the naming contract, the box round the tool, and the sizes that do not fit.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <set>
#include <string>

#include "fission/geo/error.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
namespace geo = fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// Relative tolerance 1e-6, as required for golden tests (docs/03-architektur.md §10).
Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

// Every face has exactly one role, and no two faces share one.
std::set<std::string> rolesOf(const geo::NamedSolid& body) {
    std::set<std::string> roles;
    for (const geo::NamedFace& face : body.faces) {
        roles.insert(face.role);
    }
    CHECK(roles.size() == body.faces.size());
    return roles;
}

geo::GeometryErrorCode codeOf(const geo::GeometryKernel& kernel, geo::HoleToolSpec spec) {
    try {
        kernel.makeHoleTool(spec);
    } catch (const geo::GeometryError& error) {
        return error.code();
    }
    return geo::GeometryErrorCode::kernelFailure; // not reached when the call throws
}

} // namespace

TEST_CASE("hole tool: a plain hole is a cylinder from above the face down to the depth",
          "[geometry][holes]") {
    const auto kernel = geo::occt::makeKernel();
    geo::HoleToolSpec spec; // plain, diameter 3.4, depth 10, above 1
    const geo::NamedSolid made = kernel->makeHoleTool(spec);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(kPi * 1.7 * 1.7 * 11.0));
    CHECK(rolesOf(made) == std::set<std::string>{"top", "bottom", "wall"});
    CHECK(report.boxMin.z == Approx(-10.0).margin(1e-9));
    CHECK(report.boxMax.z == Approx(1.0).margin(1e-9));
}

TEST_CASE("hole tool: a counterbore widens the top of the hole", "[geometry][holes]") {
    const auto kernel = geo::occt::makeKernel();
    geo::HoleToolSpec spec;
    spec.kind = geo::HoleKind::counterbore;
    spec.headDiameter = 6.5;
    spec.headDepth = 3.3;
    const geo::NamedSolid made = kernel->makeHoleTool(spec);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume ==
          exactly(kPi * 3.25 * 3.25 * (1.0 + 3.3) + kPi * 1.7 * 1.7 * (10.0 - 3.3)));
    const std::set<std::string> roles = rolesOf(made);
    for (const char* role : {"top", "bottom", "wall", "headWall", "headFloor"}) {
        CHECK(roles.count(role) == 1);
    }
}

TEST_CASE("hole tool: a countersink opens the top with a 90 degree cone", "[geometry][holes]") {
    const auto kernel = geo::occt::makeKernel();
    geo::HoleToolSpec spec;
    spec.kind = geo::HoleKind::countersink;
    spec.diameter = 4.5;
    spec.headDiameter = 8.96;
    const double r = 2.25;
    const double R = 4.48;
    const double h = R - r;
    const geo::NamedSolid made = kernel->makeHoleTool(spec);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(kPi * R * R * 1.0 + kPi * h / 3.0 * (R * R + R * r + r * r) +
                                   kPi * r * r * (10.0 - h)));
    CHECK(rolesOf(made).count("countersink") == 1);
}

TEST_CASE("hole tool: a nut trap is a hexagon prism over the hole", "[geometry][holes]") {
    const auto kernel = geo::occt::makeKernel();
    geo::HoleToolSpec spec;
    spec.kind = geo::HoleKind::nutTrap;
    spec.nutWidth = 5.5;
    spec.nutDepth = 2.4;
    const geo::NamedSolid made = kernel->makeHoleTool(spec);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    const double expected =
        std::sqrt(3.0) / 2.0 * 5.5 * 5.5 * (1.0 + 2.4) + kPi * 1.7 * 1.7 * (10.0 - 2.4);
    CHECK(report.volume == Approx(expected).epsilon(1e-5));
    const std::set<std::string> roles = rolesOf(made);
    for (int i = 1; i <= 6; ++i) {
        CHECK(roles.count("nutWall[" + std::to_string(i) + "]") == 1);
    }
    for (const char* role : {"top", "bottom", "wall", "nutFloor"}) {
        CHECK(roles.count(role) == 1);
    }
}

TEST_CASE("hole tool: sizes that do not fit are refused", "[geometry][holes]") {
    const auto kernel = geo::occt::makeKernel();
    geo::HoleToolSpec noHole;
    noHole.diameter = 0.0;
    CHECK(codeOf(*kernel, noHole) == geo::GeometryErrorCode::invalidInput);
    geo::HoleToolSpec narrowHead; // counterbore below the hole's diameter
    narrowHead.kind = geo::HoleKind::counterbore;
    narrowHead.headDiameter = 3.0;
    CHECK(codeOf(*kernel, narrowHead) == geo::GeometryErrorCode::invalidInput);
}

TEST_CASE("hole tool: placed at `at` along `up`, the tool stands on the face",
          "[geometry][holes]") {
    const auto kernel = geo::occt::makeKernel();
    geo::HoleToolSpec spec; // plain, diameter 3.4, depth 10, above 1
    spec.at = {10.0, 0.0, 5.0};
    spec.up = {1.0, 0.0, 0.0}; // the face looks along +X: the tool runs along X
    const geo::NamedSolid made = kernel->makeHoleTool(spec);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(kPi * 1.7 * 1.7 * 11.0)); // a pure move keeps the volume
    CHECK(rolesOf(made) == std::set<std::string>{"top", "bottom", "wall"});
    // The tool's z axis now runs along +X from `at`: x from at.x - depth to at.x + above.
    CHECK(report.boxMin.x == Approx(10.0 - 10.0).margin(1e-9));
    CHECK(report.boxMax.x == Approx(10.0 + 1.0).margin(1e-9));
    CHECK(report.boxMin.y == Approx(-1.7).margin(1e-9));
    CHECK(report.boxMax.y == Approx(1.7).margin(1e-9));
    CHECK(report.boxMin.z == Approx(5.0 - 1.7).margin(1e-9));
    CHECK(report.boxMax.z == Approx(5.0 + 1.7).margin(1e-9));

    // A direction without a length is refused.
    geo::HoleToolSpec noDirection;
    noDirection.up = {0.0, 0.0, 0.0};
    CHECK(codeOf(*kernel, noDirection) == geo::GeometryErrorCode::invalidInput);
}
