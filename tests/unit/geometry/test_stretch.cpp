// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Stretching through the kernel interface (AP-2.4b, flow D step 5): the body gets longer, holes and
// rounded corners stay exactly as they are, and a cut that would cross a detail is refused.

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
constexpr geo::Point3d kRight{1, 0, 0};

// 40 × 40 × 20 with a hole of radius 3 through the middle (the axis along z at x = 20).
geo::NamedSolid plateWithHole(const geo::GeometryKernel& kernel) {
    const geo::NamedSolid plate = kernel.makeBox(40, 40, 20, {0, 0, 0}, 0.0);
    const geo::NamedSolid pin = kernel.makeCylinder(3.0, 30.0, {20, 20, -5});
    return kernel.boolean(geo::BooleanOp::subtract, plate, {pin});
}

std::set<std::string> rolesOf(const geo::NamedSolid& body) {
    std::set<std::string> roles;
    for (const geo::NamedFace& face : body.faces) {
        roles.insert(face.role);
    }
    return roles;
}

} // namespace

TEST_CASE("stretch: the hole stays round and where it was when the plate gets wider",
          "[geometry][stretch]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);

    // The free place is right of the hole (x from 23 to 40): its middle.
    const auto cut = kernel->plainCut(plate.solid, kRight, 40.0);
    REQUIRE(cut.has_value());
    CHECK(*cut == Approx(31.5).margin(1e-3));

    const geo::NamedSolid made = kernel->stretch(plate, kRight, *cut, 20.0);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(60.0 * 40.0 * 20.0 - kPi * 9.0 * 20.0).epsilon(1e-6));
    CHECK(report.boxMax.x == Approx(60.0));
    CHECK(report.boxMin.x == Approx(0.0));

    // The hole: still one cylinder wall, radius 3, centre at x = 20 (the distance to the left edge
    // is unchanged).
    const geo::BodySignatures looks = kernel->signatures(made.solid);
    int walls = 0;
    for (const geo::ElementSignature& face : looks.faces) {
        if (face.shape == geo::SignatureShape::cylinder) {
            ++walls;
            CHECK(face.radius == Approx(3.0).epsilon(1e-9));
            CHECK(face.centroid.x == Approx(20.0).margin(1e-6));
            CHECK(face.centroid.y == Approx(20.0).margin(1e-6));
        }
    }
    CHECK(walls == 1);
    // Same six faces and the wall of the hole, with the names they had.
    CHECK(report.faces == 7);
    CHECK(rolesOf(made) == rolesOf(plate));
}

TEST_CASE("stretch: a negative distance takes a piece out", "[geometry][stretch]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);
    const geo::NamedSolid made = kernel->stretch(plate, kRight, 31.5, -5.0);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(35.0 * 40.0 * 20.0 - kPi * 9.0 * 20.0).epsilon(1e-6));
    CHECK(report.boxMax.x == Approx(35.0));
    CHECK(rolesOf(made) == rolesOf(plate));

    // The piece taken out must not reach the hole (it would be shortened, not kept): refused.
    CHECK_THROWS_AS(kernel->stretch(plate, kRight, 31.5, -10.0), geo::GeometryError);
}

TEST_CASE("stretch: stretching to the left works the same", "[geometry][stretch]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);
    const geo::Point3d left{-1, 0, 0};
    const auto cut = kernel->plainCut(plate.solid, left, 0.0);
    REQUIRE(cut.has_value());
    // A coordinate along the way (−x): the free place left of the hole is x from 0 to 17.
    CHECK(*cut == Approx(-8.5).margin(1e-3));
    const geo::NamedSolid made = kernel->stretch(plate, left, *cut, 15.0);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.boxMin.x == Approx(-15.0));
    CHECK(report.volume == Approx(55.0 * 40.0 * 20.0 - kPi * 9.0 * 20.0).epsilon(1e-6));
}

TEST_CASE("stretch: rounded corners stay round", "[geometry][stretch]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid rounded = kernel->makeBox(40, 40, 20, {0, 0, 0}, 4.0);
    const double before = kernel->measure(rounded.solid).volume;
    const auto cut = kernel->plainCut(rounded.solid, kRight, 40.0);
    REQUIRE(cut.has_value());
    CHECK(*cut > 4.0);
    CHECK(*cut < 36.0);
    const geo::NamedSolid made = kernel->stretch(rounded, kRight, *cut, 10.0);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(before + 10.0 * 40.0 * 20.0).epsilon(1e-6));
    CHECK(report.faces == 10); // the corners are still four round walls
    const geo::BodySignatures looks = kernel->signatures(made.solid);
    for (const geo::ElementSignature& face : looks.faces) {
        if (face.shape == geo::SignatureShape::cylinder) {
            CHECK(face.radius == Approx(4.0).epsilon(1e-9));
        }
    }
}

TEST_CASE("stretch: a cut through the hole or a bad request is refused", "[geometry][stretch]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);
    try {
        (void)kernel->stretch(plate, kRight, 20.0, 5.0); // right through the hole
        FAIL("a cut through the hole must be refused");
    } catch (const geo::GeometryError& error) {
        CHECK(error.code() == geo::GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "position");
    }
    CHECK_THROWS_AS(kernel->stretch(plate, kRight, 31.5, 0.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->stretch(plate, kRight, 31.5, -60.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->stretch(plate, kRight, 500.0, 5.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->stretch(plate, {0.6, 0.8, 0}, 10.0, 5.0), geo::GeometryError);
    CHECK_FALSE(kernel->plainCut(plate.solid, {0.6, 0.8, 0}, 10.0).has_value());

    // A ball has no place without a curve.
    const geo::NamedSolid ball = kernel->makePrimitive(geo::SphereSpec{}, {0, 0, 0});
    CHECK_FALSE(kernel->plainCut(ball.solid, kRight, 0.0).has_value());
}
