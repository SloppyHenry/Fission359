// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pushing and pulling faces through the kernel interface (AP-2.4a): a flat wall moves, the wall of
// a hole or a pin gets another radius, volumes come from the formula, names stay, and faces that
// cannot be moved are refused instead of made wrong.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

TEST_CASE("faceAnchor: a flat face looks out, a hole looks into itself, a pin out",
          "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 20, 10, {0, 0, 0}, 0.0);
    const geo::FaceAnchor right = kernel->faceAnchor(box.solid, flatFace(*kernel, box, {1, 0, 0}));
    CHECK(right.point.x == Approx(20.0));
    CHECK(right.way.x == Approx(1.0));

    // The wall of a hole looks towards the axis: from the point on the wall the axis lies along
    // the way.
    const geo::NamedSolid plate = plateWithHole(*kernel);
    const geo::FaceAnchor hole = kernel->faceAnchor(plate.solid, roundFace(*kernel, plate, 3.0));
    const double toAxisX = 20.0 - hole.point.x;
    const double toAxisY = 20.0 - hole.point.y;
    CHECK(toAxisX * hole.way.x + toAxisY * hole.way.y == Approx(3.0).epsilon(1e-9));

    const geo::NamedSolid cylinder = kernel->makeCylinder(5.0, 10.0, {0, 0, 0});
    const geo::FaceAnchor pin =
        kernel->faceAnchor(cylinder.solid, roundFace(*kernel, cylinder, 5.0));
    CHECK(pin.point.x * pin.way.x + pin.point.y * pin.way.y == Approx(5.0).epsilon(1e-9));
}

TEST_CASE("removeFaces: the wall of a hole is removed and the hole closes", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);
    const geo::NamedSolid made = kernel->removeFaces(plate, {roundFace(*kernel, plate, 3.0)});
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(40.0 * 40.0 * 20.0).epsilon(1e-9));
    CHECK(report.faces == 6);
    // The six faces of the plate keep their names; the wall is listed as gone.
    std::set<std::string> roles = rolesOf(made);
    CHECK(roles.size() == 6);
    CHECK(roles.count("side") == 0);
    CHECK(made.history.deleted.size() == 1);
    CHECK(made.history.deleted[0] == "side");
}

TEST_CASE("removeFaces: rounded corners become sharp again", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid rounded = kernel->makeBox(40, 40, 20, {0, 0, 0}, 4.0);
    std::vector<std::uint32_t> corners;
    const geo::BodySignatures looks = kernel->signatures(rounded.solid);
    for (const geo::ElementSignature& face : looks.faces) {
        if (face.shape == geo::SignatureShape::cylinder) {
            corners.push_back(face.id);
        }
    }
    REQUIRE(corners.size() == 4);
    const geo::NamedSolid made = kernel->removeFaces(rounded, corners);
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(40.0 * 40.0 * 20.0).epsilon(1e-9));
    CHECK(report.faces == 6);
    CHECK(rolesOf(made).size() == 6);

    CHECK_THROWS_AS(kernel->removeFaces(rounded, {}), geo::GeometryError);
    CHECK_THROWS_AS(kernel->removeFaces(rounded, {999}), geo::GeometryError);
}

TEST_CASE("offsetFaces: all faces of a box moved in make it smaller on every side",
          "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(20, 30, 10, {0, 0, 0}, 0.0);
    const geo::NamedSolid inner = kernel->offsetFaces(box, {1, 2, 3, 4, 5, 6}, -0.2);
    const geo::SolidReport report = kernel->measure(inner.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(19.6 * 29.6 * 9.6).epsilon(1e-9));
    CHECK(report.faces == 6);
    CHECK(report.boxMin.x == Approx(0.2).margin(1e-9));
    CHECK(report.boxMax.x == Approx(19.8).margin(1e-9));
    CHECK(rolesOf(inner) == rolesOf(box));

    const geo::NamedSolid outer = kernel->offsetFaces(box, {1, 2, 3, 4, 5, 6}, 0.2);
    CHECK(kernel->measure(outer.solid).valid);
    CHECK(kernel->measure(outer.solid).volume == Approx(20.4 * 30.4 * 10.4).epsilon(1e-9));
    CHECK(kernel->measure(outer.solid).faces == 6);
    CHECK_THROWS_AS(kernel->offsetFaces(box, {}, 0.2), geo::GeometryError);
}

TEST_CASE("tiltFace: a wall turned about its bottom edge makes a wedge", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const std::uint32_t right = flatFace(*kernel, cube, {1, 0, 0});
    const geo::Point3d hinge{20, 0, 0};
    const geo::Point3d along{0, 1, 0};

    // Turned right-handed about +y the face leans out at the top (x = 20 + z for 45 degrees).
    const geo::NamedSolid out = kernel->tiltFace(cube, right, hinge, along, 45.0);
    const geo::SolidReport outReport = kernel->measure(out.solid);
    CHECK(outReport.valid);
    CHECK(outReport.volume == Approx(8000.0 + 0.5 * 20.0 * 20.0 * 20.0).epsilon(1e-9));
    CHECK(outReport.boxMax.x == Approx(40.0));
    CHECK(rolesOf(out) == rolesOf(cube));

    // The other way it leans in: a piece the size of the wedge is cut off.
    const geo::NamedSolid in = kernel->tiltFace(cube, right, hinge, along, -45.0);
    const geo::SolidReport inReport = kernel->measure(in.solid);
    CHECK(inReport.valid);
    CHECK(inReport.volume == Approx(8000.0 - 0.5 * 20.0 * 20.0 * 20.0).epsilon(1e-9));
    // At 45 degrees the leaning wall runs through the top edge: the top is gone, a triangle
    // remains.
    CHECK(inReport.faces == 5);
    CHECK(std::count(in.history.deleted.begin(), in.history.deleted.end(), "top") == 1);

    // A smaller angle, a different wedge: tan(30 degrees) * 20 * 20 / 2 * 20; all six faces stay.
    const geo::NamedSolid slight = kernel->tiltFace(cube, right, hinge, along, -30.0);
    CHECK(kernel->measure(slight.solid).valid);
    CHECK(kernel->measure(slight.solid).faces == 6);
    CHECK(kernel->measure(slight.solid).volume ==
          Approx(8000.0 - 0.5 * 20.0 * 20.0 * std::tan(30.0 * kPi / 180.0) * 20.0).epsilon(1e-9));
    CHECK(rolesOf(slight) == rolesOf(cube));
}

TEST_CASE("tiltFace: a hinge off the face, no angle or a face that is not flat are refused",
          "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const std::uint32_t right = flatFace(*kernel, cube, {1, 0, 0});
    CHECK_THROWS_AS(kernel->tiltFace(cube, right, {20, 0, 0}, {0, 1, 0}, 0.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->tiltFace(cube, right, {20, 0, 0}, {0, 1, 0}, 90.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->tiltFace(cube, right, {5, 0, 0}, {0, 1, 0}, 10.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->tiltFace(cube, right, {20, 0, 0}, {1, 0, 0}, 10.0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->tiltFace(cube, 99, {20, 0, 0}, {0, 1, 0}, 10.0), geo::GeometryError);

    const geo::NamedSolid cylinder = kernel->makeCylinder(5.0, 10.0, {0, 0, 0});
    CHECK_THROWS_AS(
        kernel->tiltFace(cylinder, roundFace(*kernel, cylinder, 5.0), {5, 0, 0}, {0, 1, 0}, 10.0),
        geo::GeometryError);
}

TEST_CASE("flatFace: the top of a box has its middle, its normal and its longest edge",
          "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(40, 20, 10, {0, 0, 0}, 0.0);
    const geo::FlatFace top = kernel->flatFace(box.solid, flatFace(*kernel, box, {0, 0, 1}));
    CHECK(top.centre.x == Approx(20.0));
    CHECK(top.centre.y == Approx(10.0));
    CHECK(top.centre.z == Approx(10.0));
    CHECK(top.normal.z == Approx(1.0));
    REQUIRE(top.longestEdge.has_value());
    CHECK(std::abs(top.longestEdge->x) == Approx(1.0)); // the 40 mm edges
    CHECK(top.longestEdge->z == Approx(0.0).margin(1e-12));
}

TEST_CASE("flatFace: on every side of a box the edge runs along the longer side of the face",
          "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid box = kernel->makeBox(40, 20, 10, {0, 0, 0}, 0.0);
    // Direction of the face, and the axis (0 = x, 1 = y, 2 = z) its longest edge must run along.
    const struct {
        geo::Point3d looks;
        int along;
    } sides[] = {{{0, 0, 1}, 0},  {{0, 0, -1}, 0}, {{0, 1, 0}, 0},
                 {{0, -1, 0}, 0}, {{1, 0, 0}, 1},  {{-1, 0, 0}, 1}};
    for (const auto& side : sides) {
        const geo::FlatFace face = kernel->flatFace(box.solid, flatFace(*kernel, box, side.looks));
        REQUIRE(face.longestEdge.has_value());
        const double parts[3] = {face.longestEdge->x, face.longestEdge->y, face.longestEdge->z};
        CHECK(std::abs(parts[side.along]) == Approx(1.0));
        CHECK(face.normal.x == Approx(side.looks.x));
        CHECK(face.normal.y == Approx(side.looks.y));
        CHECK(face.normal.z == Approx(side.looks.z));
    }
}

TEST_CASE("flatFace: only the outer boundary counts, not the edge of a hole", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid plate = plateWithHole(*kernel);
    const geo::FlatFace top = kernel->flatFace(plate.solid, flatFace(*kernel, plate, {0, 0, 1}));
    CHECK(top.centre.x == Approx(20.0));
    CHECK(top.centre.y == Approx(20.0));
    CHECK(top.normal.z == Approx(1.0));
    // The outer edges are straight and 40 mm long; the circle of the hole has no straight edge.
    REQUIRE(top.longestEdge.has_value());
    CHECK(std::abs(top.longestEdge->x) + std::abs(top.longestEdge->y) == Approx(1.0));
}

TEST_CASE("flatFace: a face without a straight edge has none to offer", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cylinder = kernel->makeCylinder(5.0, 10.0, {0, 0, 0});
    const geo::FlatFace top =
        kernel->flatFace(cylinder.solid, flatFace(*kernel, cylinder, {0, 0, 1}));
    CHECK(top.normal.z == Approx(1.0));
    CHECK_FALSE(top.longestEdge.has_value());
}

TEST_CASE("flatFace: a curved face and a face that is not there are refused", "[geometry][faces]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cylinder = kernel->makeCylinder(5.0, 10.0, {0, 0, 0});
    CHECK_THROWS_AS(kernel->flatFace(cylinder.solid, roundFace(*kernel, cylinder, 5.0)),
                    geo::GeometryError);
    CHECK_THROWS_AS(kernel->flatFace(cylinder.solid, 0), geo::GeometryError);
    CHECK_THROWS_AS(kernel->flatFace(cylinder.solid, 99), geo::GeometryError);
}
