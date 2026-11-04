// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pushing and pulling faces through the kernel interface (AP-2.4a): a flat wall moves, the wall of
// a hole or a pin gets another radius, volumes come from the formula, names stay, and faces that
// cannot be moved are refused instead of made wrong.

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

// The number of the first face that lies in `shape` and looks along `direction` (flat faces) or
// has the given radius (round faces, `direction` ignored then).
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

std::uint32_t roundFace(const geo::GeometryKernel& kernel, const geo::NamedSolid& body,
                        double radius) {
    const geo::BodySignatures looks = kernel.signatures(body.solid);
    for (const geo::ElementSignature& face : looks.faces) {
        if (face.shape == geo::SignatureShape::cylinder && std::abs(face.radius - radius) < 1e-6) {
            return face.id;
        }
    }
    FAIL("no such round face");
    return 0;
}

std::set<std::string> rolesOf(const geo::NamedSolid& body) {
    std::set<std::string> roles;
    for (const geo::NamedFace& face : body.faces) {
        roles.insert(face.role);
    }
    return roles;
}

// A plate 40 x 40 x 20 with a hole of radius 3 through its middle.
geo::NamedSolid plateWithHole(const geo::GeometryKernel& kernel) {
    const geo::NamedSolid plate = kernel.makeBox(40, 40, 20, {0, 0, 0}, 0.0);
    const geo::NamedSolid pin = kernel.makeCylinder(3.0, 30.0, {20, 20, -5});
    return kernel.boolean(geo::BooleanOp::subtract, plate, {pin});
}

} // namespace

TEST_CASE("offsetFace: pulling a flat wall out makes the box longer", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const geo::NamedSolid made = kernel->offsetFace(box, flatFace(*kernel, box, {1, 0, 0}), 5.0);

    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(25.0 * 20.0 * 10.0).epsilon(1e-9));
    CHECK(report.faces == 6);
    CHECK(report.boxMax.x == Approx(25.0));
    // Every face keeps the name it had (the wall moved, it was not replaced).
    CHECK(rolesOf(made) == rolesOf(box));
    CHECK(made.faces.size() == 6);
}

TEST_CASE("offsetFace: pushing a flat wall in makes the box shorter", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const geo::NamedSolid made = kernel->offsetFace(box, flatFace(*kernel, box, {0, 0, 1}), -4.0);

    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(20.0 * 20.0 * 6.0).epsilon(1e-9));
    CHECK(report.boxMax.z == Approx(6.0));
    CHECK(rolesOf(made) == rolesOf(box));
}

TEST_CASE("offsetFace: the wall of a hole gives a wider or a narrower hole", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);
    const std::uint32_t wall = roundFace(*kernel, plate, 3.0);

    // The wall looks into the hole: −1 takes material away (radius 4), +1 adds some (radius 2).
    const geo::NamedSolid wider = kernel->offsetFace(plate, wall, -1.0);
    const geo::SolidReport widerReport = kernel->measure(wider.solid);
    CHECK(widerReport.valid);
    CHECK(widerReport.volume == Approx(32000.0 - kPi * 16.0 * 20.0).epsilon(1e-6));
    CHECK(widerReport.faces == 7);
    CHECK(rolesOf(wider) == rolesOf(plate));
    // The hole stays where it was and round: the new wall is a cylinder of radius 4.
    const geo::BodySignatures looks = kernel->signatures(wider.solid);
    bool found = false;
    for (const geo::ElementSignature& face : looks.faces) {
        if (face.shape == geo::SignatureShape::cylinder) {
            found = true;
            CHECK(face.radius == Approx(4.0).epsilon(1e-9));
            CHECK(face.centroid.x == Approx(20.0).margin(1e-6));
            CHECK(face.centroid.y == Approx(20.0).margin(1e-6));
        }
    }
    CHECK(found);

    const geo::NamedSolid narrower = kernel->offsetFace(plate, wall, 1.0);
    const geo::SolidReport narrowerReport = kernel->measure(narrower.solid);
    CHECK(narrowerReport.valid);
    CHECK(narrowerReport.volume == Approx(32000.0 - kPi * 4.0 * 20.0).epsilon(1e-6));
    CHECK(rolesOf(narrower) == rolesOf(plate));
}

TEST_CASE("offsetFace: the side of a pin gets thicker or thinner", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cylinder = kernel->makeCylinder(5.0, 10.0, {0, 0, 0});
    const std::uint32_t side = roundFace(*kernel, cylinder, 5.0);

    const geo::NamedSolid thicker = kernel->offsetFace(cylinder, side, 2.0);
    CHECK(kernel->measure(thicker.solid).valid);
    CHECK(kernel->measure(thicker.solid).volume == Approx(kPi * 49.0 * 10.0).epsilon(1e-6));
    CHECK(rolesOf(thicker) == rolesOf(cylinder));

    const geo::NamedSolid thinner = kernel->offsetFace(cylinder, side, -2.0);
    CHECK(kernel->measure(thinner.solid).volume == Approx(kPi * 9.0 * 10.0).epsilon(1e-6));
    CHECK(rolesOf(thinner) == rolesOf(cylinder));
}

TEST_CASE("offsetFace: faces that cannot be moved are refused, not made wrong",
          "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const std::uint32_t top = flatFace(*kernel, box, {0, 0, 1});

    CHECK_THROWS_AS(kernel->offsetFace(box, top, 0.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->offsetFace(box, top, std::nan("")), geo::GeometryError);
    CHECK_THROWS_AS(kernel->offsetFace(box, 99, 1.0), geo::GeometryError);
    // Taking away more than the box has.
    CHECK_THROWS_AS(kernel->offsetFace(box, top, -30.0), geo::GeometryError);

    // A hole cannot become narrower than nothing.
    const geo::NamedSolid plate = plateWithHole(*kernel);
    CHECK_THROWS_AS(kernel->offsetFace(plate, roundFace(*kernel, plate, 3.0), 3.0),
                    geo::GeometryError);

    // A sphere has a face that is neither flat nor a cylinder wall.
    const geo::NamedSolid ball = kernel->makePrimitive(geo::SphereSpec{}, {0, 0, 0});
    CHECK_THROWS_AS(kernel->offsetFace(ball, 1, 1.0), geo::GeometryError);
}
