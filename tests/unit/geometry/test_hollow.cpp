// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Hollowing through the kernel interface (AP-2.6): volumes from the formula (never from the kernel
// itself), the faces keep their names, a wall that is too thick says so, and a body that cannot be
// rebuilt is either hollowed in a way that passes the check or refused as "not sure".

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <set>

#include "fission/geo/occt/kernel.h"

using Catch::Approx;
namespace geo = fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

std::uint32_t flatFace(const geo::GeometryKernel& kernel, const geo::NamedSolid& body,
                       geo::Point3d direction) {
    const geo::BodySignatures looks = kernel.signatures(body.solid);
    for (const geo::ElementSignature& face : looks.faces) {
        if (face.shape == geo::SignatureShape::plane &&
            std::abs(face.direction.x - direction.x) < 1e-6 &&
            std::abs(face.direction.y - direction.y) < 1e-6 &&
            std::abs(face.direction.z - direction.z) < 1e-6) {
            return face.id;
        }
    }
    FAIL("no such flat face");
    return 0;
}

std::set<std::string> rolesOf(const geo::NamedSolid& body) {
    std::set<std::string> roles;
    for (const geo::NamedFace& face : body.faces) {
        roles.insert(face.role);
    }
    return roles;
}

// Footprint of a box with round corners (mm²).
double footprint(double width, double depth, double radius) {
    return width * depth - (4.0 - kPi) * radius * radius;
}

} // namespace

TEST_CASE("hollow: a box with round corners and an open top takes the formula's volume",
          "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(40, 40, 20, {0, 0, 0}, 5.0);
    const std::uint32_t top = flatFace(*kernel, box, {0, 0, 1});

    // Wall 2: the inner corners are round with radius 3, the floor is 2 thick.
    const geo::NamedSolid made = kernel->hollow(box, {top}, 2.0, false, "F2/shell/inner");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.solids == 1);
    CHECK(report.shells == 1);
    CHECK(report.volume ==
          Approx(footprint(40, 40, 5) * 20.0 - footprint(36, 36, 3) * 18.0).epsilon(1e-6));
    // The faces of the body keep their names (the open top stays as the rim of the wall, still
    // called "top"), the faces of the cavity are named after the place they follow.
    const std::set<std::string> roles = rolesOf(made);
    CHECK(roles.size() == made.faces.size());
    CHECK(roles.count("bottom") == 1);
    CHECK(roles.count("cornerFrontLeft") == 1);
    CHECK(roles.count("top") == 1);
    CHECK(roles.count("F2/shell/inner[bottom]") == 1);
    CHECK(roles.count("F2/shell/inner[cornerBackRight]") == 1);
    // Nine outside faces, the rim, and nine faces of the cavity (floor, four sides, four corners).
    CHECK(report.faces == 19);
}

TEST_CASE("hollow: a wall thicker than the corner radius gives sharp inner corners",
          "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(40, 40, 20, {0, 0, 0}, 5.0);
    const geo::NamedSolid made =
        kernel->hollow(box, {flatFace(*kernel, box, {0, 0, 1})}, 6.0, false, "F2/shell/inner");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(footprint(40, 40, 5) * 20.0 - 28.0 * 28.0 * 14.0).epsilon(1e-6));
}

TEST_CASE("hollow: a cylinder with an open top", "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cylinder = kernel->makeCylinder(15.0, 30.0, {0, 0, 0});
    const geo::NamedSolid made = kernel->hollow(cylinder, {flatFace(*kernel, cylinder, {0, 0, 1})},
                                                2.0, false, "F2/shell/inner");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.shells == 1);
    CHECK(report.volume == Approx(kPi * (225.0 * 30.0 - 169.0 * 28.0)).epsilon(1e-6));
    CHECK(rolesOf(made).count("side") == 1);
    CHECK(rolesOf(made).count("F2/shell/inner[side]") == 1);
}

TEST_CASE("hollow: no open face gives a closed hollow body with two shells", "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(40, 40, 20, {0, 0, 0}, 0.0);
    const geo::NamedSolid made = kernel->hollow(box, {}, 2.0, false, "F2/shell/inner");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.shells == 2);
    CHECK(report.volume == Approx(32000.0 - 36.0 * 36.0 * 16.0).epsilon(1e-9));
    CHECK(report.faces == 12);
}

TEST_CASE("hollow: the wall can grow outwards instead", "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const geo::NamedSolid made =
        kernel->hollow(box, {flatFace(*kernel, box, {0, 0, 1})}, 2.0, true, "F2/shell/inner");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    // The body is the cavity; the walls and the floor are added around it (the top stays open).
    CHECK(report.volume == Approx(24.0 * 24.0 * 22.0 - 20.0 * 20.0 * 20.0).epsilon(1e-9));
    CHECK(report.boxMax.z == Approx(20.0));
    CHECK(report.boxMin.z == Approx(-2.0));
    // The outside keeps the names of the body, the cavity is named after them.
    const std::set<std::string> roles = rolesOf(made);
    CHECK(roles.count("bottom") == 1);
    CHECK(roles.count("left") == 1);
    CHECK(roles.count("F2/shell/inner[bottom]") == 1);
}

TEST_CASE("hollow: a wall that is too thick names the thinnest place and the largest wall",
          "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 30, 40, {0, 0, 0}, 0.0);
    const std::uint32_t top = flatFace(*kernel, box, {0, 0, 1});
    try {
        (void)kernel->hollow(box, {top}, 10.0, false, "F2/shell/inner");
        FAIL("ten millimetres of wall in a box 20 wide must be refused");
    } catch (const geo::GeometryError& error) {
        CHECK(error.code() == geo::GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "thickness");
        CHECK(error.numbers().at("thinnest") == Approx(20.0));
        CHECK(error.numbers().at("maxValue") == Approx(9.9));
    }
    // The largest wall that is named does work.
    CHECK(kernel->measure(kernel->hollow(box, {top}, 9.9, false, "F2/shell/inner").solid).valid);
    CHECK_THROWS_AS(kernel->hollow(box, {top}, 0.0, false, "x"), geo::GeometryError);
    CHECK_THROWS_AS(kernel->hollow(box, {99}, 2.0, false, "x"), geo::GeometryError);
}

TEST_CASE("hollow: a body that cannot be rebuilt is hollowed with a check, or refused as not sure",
          "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    // A plate with a round hole is no box any more: the kernel's own call is used.
    const geo::NamedSolid plate = kernel->makeBox(40, 40, 20, {0, 0, 0}, 0.0);
    const geo::NamedSolid pin = kernel->makeCylinder(5.0, 30.0, {20, 20, -5});
    const geo::NamedSolid holed = kernel->boolean(geo::BooleanOp::subtract, plate, {pin});
    const std::uint32_t top = flatFace(*kernel, holed, {0, 0, 1});
    try {
        const geo::NamedSolid made = kernel->hollow(holed, {top}, 1.0, false, "F3/shell/inner");
        const geo::SolidReport report = kernel->measure(made.solid);
        // Whatever comes back must have passed the check: valid, one body, one shell, and a wall
        // volume near area times wall.
        CHECK(report.valid);
        CHECK(report.solids == 1);
        CHECK(report.shells == 1);
        std::set<std::string> roles;
        for (const geo::NamedFace& face : made.faces) {
            CHECK(roles.insert(face.role).second);
        }
    } catch (const geo::GeometryError& error) {
        CHECK(error.code() == geo::GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "unsure");
    }

    // A closed hollow body is not made for such a body: it is refused, not guessed.
    try {
        (void)kernel->hollow(holed, {}, 1.0, false, "F3/shell/inner");
        FAIL("a closed hollow body of an unknown shape must be refused");
    } catch (const geo::GeometryError& error) {
        CHECK(error.subject() == "unsure");
    }
}

// The known problem of the kernel report (case C2): hollowing a box with all edges rounded. The
// kernel's own call can return a body that is valid and wrong. Whatever this returns must have
// passed the plausibility check; otherwise the answer is "not sure" and nothing is made.
TEST_CASE("hollow: a box with all edges rounded never gives a valid but wrong body",
          "[geometry][hollow]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(40, 40, 20, {0, 0, 0}, 0.0);
    std::vector<geo::BlendEdge> edges;
    for (const geo::ElementSignature& edge : kernel->signatures(box.solid).edges) {
        if (edge.id != 0) {
            edges.push_back({edge.id, "e" + std::to_string(edge.id)});
        }
    }
    const geo::NamedSolid rounded =
        kernel->blendEdges(box, edges, {geo::BlendKind::round, 4.0, 0.0}, "F2/fillet/round");
    const geo::SolidReport before = kernel->measure(rounded.solid);
    const std::uint32_t top = flatFace(*kernel, rounded, {0, 0, 1});

    for (const double wall : {2.0, 3.0}) {
        try {
            const geo::NamedSolid made =
                kernel->hollow(rounded, {top}, wall, false, "F3/shell/inner");
            const geo::SolidReport report = kernel->measure(made.solid);
            CHECK(report.valid);
            CHECK(report.solids == 1);
            CHECK(report.shells == 1);
            CHECK(report.volume < before.volume);
            // The wall is about as large as its area times its thickness (the check's own claim).
            CHECK(report.volume > 0.5 * (before.area - 40.0 * 40.0) * wall * 0.5);
        } catch (const geo::GeometryError& error) {
            CHECK(error.code() == geo::GeometryErrorCode::invalidInput);
            CHECK(error.subject() == "unsure");
        }
    }
}
