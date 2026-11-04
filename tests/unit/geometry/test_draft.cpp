// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Draft through the kernel interface (AP-2.6): the four sides of a box tilted by 5 degrees give a
// frustum with the volume of its formula; faces keep their names; unfit requests are refused.

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

std::vector<std::uint32_t> sidesOf(const geo::GeometryKernel& kernel, const geo::NamedSolid& body) {
    std::vector<std::uint32_t> sides;
    for (const geo::ElementSignature& face : kernel.signatures(body.solid).faces) {
        if (face.shape == geo::SignatureShape::plane && std::abs(face.direction.z) < 0.5) {
            sides.push_back(face.id);
        }
    }
    return sides;
}

std::uint32_t faceLooking(const geo::GeometryKernel& kernel, const geo::NamedSolid& body,
                          double z) {
    for (const geo::ElementSignature& face : kernel.signatures(body.solid).faces) {
        if (face.shape == geo::SignatureShape::plane && std::abs(face.direction.z - z) < 1e-9) {
            return face.id;
        }
    }
    FAIL("no such face");
    return 0;
}

} // namespace

TEST_CASE("draft: the four sides of a box tilted by 5 degrees make a frustum",
          "[geometry][draft]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const std::vector<std::uint32_t> sides = sidesOf(*kernel, box);
    REQUIRE(sides.size() == 4);

    // The bottom is the neutral plane; positive angles make the body narrower going up.
    const geo::NamedSolid made = kernel->draftFaces(box, sides, {0, 0, 0}, {0, 0, 1}, 5.0);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.faces == 6);
    const double top = 20.0 - 2.0 * 10.0 * std::tan(5.0 * kPi / 180.0);
    const double frustum = 10.0 / 3.0 * (400.0 + top * top + 20.0 * top);
    CHECK(report.volume == Approx(frustum).epsilon(1e-6));
    CHECK(report.boxMax.x == Approx(20.0).margin(1e-6)); // the bottom is as wide as before
    // Every face keeps its name.
    std::set<std::string> roles;
    for (const geo::NamedFace& face : made.faces) {
        roles.insert(face.role);
    }
    CHECK(roles.size() == 6);
    CHECK(roles.count("left") == 1);
    CHECK(roles.count("top") == 1);

    // The other sign makes the body wider going up.
    const geo::NamedSolid wider = kernel->draftFaces(box, sides, {0, 0, 0}, {0, 0, 1}, -5.0);
    CHECK(kernel->measure(wider.solid).valid);
    const double widerTop = 20.0 + 2.0 * 10.0 * std::tan(5.0 * kPi / 180.0);
    CHECK(kernel->measure(wider.solid).volume ==
          Approx(10.0 / 3.0 * (400.0 + widerTop * widerTop + 20.0 * widerTop)).epsilon(1e-6));
}

TEST_CASE("draft: one side leans, the others stay", "[geometry][draft]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const std::vector<std::uint32_t> sides = sidesOf(*kernel, box);
    const geo::NamedSolid made =
        kernel->draftFaces(box, {sides.front()}, {0, 0, 0}, {0, 0, 1}, 5.0);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    // A wedge of 20 wide, 10 high and tan(5 degrees) * 10 deep is taken off.
    CHECK(report.volume ==
          Approx(4000.0 - 0.5 * 10.0 * 10.0 * std::tan(5.0 * kPi / 180.0) * 20.0).epsilon(1e-6));
}

TEST_CASE("draft: bad requests are refused", "[geometry][draft]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const std::vector<std::uint32_t> sides = sidesOf(*kernel, box);
    CHECK_THROWS_AS(kernel->draftFaces(box, sides, {0, 0, 0}, {0, 0, 1}, 0.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->draftFaces(box, sides, {0, 0, 0}, {0, 0, 1}, 60.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->draftFaces(box, {}, {0, 0, 0}, {0, 0, 1}, 5.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->draftFaces(box, {99}, {0, 0, 0}, {0, 0, 1}, 5.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->draftFaces(box, sides, {0, 0, 0}, {0, 0, 0}, 5.0), geo::GeometryError);
    // The top stands square to the pull: it cannot lean.
    CHECK_THROWS_AS(
        kernel->draftFaces(box, {faceLooking(*kernel, box, 1.0)}, {0, 0, 0}, {0, 0, 1}, 5.0),
        geo::GeometryError);
}
