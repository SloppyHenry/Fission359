// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Turning (AP-1.6b): the maths of the three angles, and the kernel turning a body without
// changing which face is which (golden tests: volume, area, box, validity, names).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <map>
#include <string>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"
#include "fission/geo/rotation.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

Approx near0() {
    return Approx(0.0).margin(1e-9);
}

void checkSameTurn(const Matrix3& a, const Matrix3& b) {
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            CHECK(a.m[r][c] == Approx(b.m[r][c]).margin(1e-9));
        }
    }
}

// The average outward direction of the triangles of one face of a mesh.
Point3d averageNormal(const TriangleMesh& mesh, std::uint32_t faceId) {
    Point3d sum;
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        if (mesh.faceIds[t] != faceId) {
            continue;
        }
        const Point3f& a = mesh.positions[mesh.indices[3 * t]];
        const Point3f& b = mesh.positions[mesh.indices[3 * t + 1]];
        const Point3f& c = mesh.positions[mesh.indices[3 * t + 2]];
        const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        sum.x += uy * vz - uz * vy; // twice the area, along the normal
        sum.y += uz * vx - ux * vz;
        sum.z += ux * vy - uy * vx;
    }
    const double length = std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
    return {sum.x / length, sum.y / length, sum.z / length};
}

} // namespace

TEST_CASE("turning: a quarter turn around each axis sends the axes where they belong",
          "[rotation]") {
    const Point3d x = rotationOf({0, 0, 90}).apply({1, 0, 0});
    CHECK(x.x == near0());
    CHECK(x.y == exactly(1.0));
    const Point3d y = rotationOf({90, 0, 0}).apply({0, 1, 0});
    CHECK(y.z == exactly(1.0));
    const Point3d z = rotationOf({0, 90, 0}).apply({0, 0, 1});
    CHECK(z.x == exactly(1.0));
}

TEST_CASE("turning: X acts first, then Y, then Z", "[rotation]") {
    // Up (+Z) tipped 90° around X goes to -Y; then 90° around Z takes -Y to +X.
    const Point3d up = rotationOf({90, 0, 90}).apply({0, 0, 1});
    CHECK(up.x == exactly(1.0));
    CHECK(up.y == near0());
    CHECK(up.z == near0());
}

TEST_CASE("turning: the angles of a turn come back as they were put in", "[rotation]") {
    const Angles put = GENERATE(Angles{0, 0, 0}, Angles{30, 0, 0}, Angles{0, 45, 0},
                                Angles{0, 0, 135}, Angles{10, 20, 30}, Angles{-170, 80, 120},
                                Angles{90, -60, -45}, Angles{15, -89, 15});
    CAPTURE(put.x, put.y, put.z);
    const Angles back = anglesOf(rotationOf(put), put);
    CHECK(back.x == Approx(put.x).margin(1e-6));
    CHECK(back.y == Approx(put.y).margin(1e-6));
    CHECK(back.z == Approx(put.z).margin(1e-6));
}

TEST_CASE("turning: whatever the hint, the angles give the same turn", "[rotation]") {
    const Matrix3 turn = rotationOf({33, 71, -122});
    for (const Angles& hint : {Angles{}, Angles{180, 0, 180}, Angles{-90, -90, 10}}) {
        checkSameTurn(rotationOf(anglesOf(turn, hint)), turn);
    }
}

TEST_CASE("turning: Y at plus or minus 90 degrees keeps Z and still gives the same turn",
          "[rotation]") {
    for (const double y : {90.0, -90.0}) {
        const Matrix3 turn = rotationOf({25, y, 40});
        const Angles back = anglesOf(turn, {25, y, 40});
        CHECK(back.z == Approx(40.0).margin(1e-6));
        checkSameTurn(rotationOf(back), turn);
    }
}

TEST_CASE("turning: a turn around Z on a body that was only tipped changes one value",
          "[rotation]") {
    const Angles start{30, 0, 0};
    const Angles after = turnedAbout(start, 2, 45.0, start);
    CHECK(after.x == Approx(30.0).margin(1e-9));
    CHECK(after.y == near0());
    CHECK(after.z == Approx(45.0).margin(1e-9));
}

TEST_CASE("turning: a ring turn around X on a body already spun around Z gives the same body "
          "as turning the world",
          "[rotation]") {
    const Angles start{0, 0, 90};
    const Angles after = turnedAbout(start, 0, 90.0, start);
    checkSameTurn(rotationOf(after), axisRotation(0, 90.0) * rotationOf(start));
}

TEST_CASE("turning: smooth values pass 180 degrees without jumping", "[rotation]") {
    Angles last{0, 0, 170};
    for (double delta = 0.0; delta <= 40.0; delta += 5.0) {
        const Angles now = turnedAbout({0, 0, 170}, 2, delta, last);
        CHECK(std::abs(now.z - last.z) <= 5.0 + 1e-9);
        last = now;
    }
    CHECK(last.z == Approx(210.0).margin(1e-6));
}

TEST_CASE("turning: angles that are zero come out as zero", "[rotation]") {
    const Angles a = anglesOf(axisRotation(2, 360.0), {});
    CHECK(a.isZero());
}

TEST_CASE("kernel turn: a box turned a quarter around X lies on its side", "[solid][golden]") {
    // 20 wide, 10 deep, 40 high, standing on the plane. Turned about its middle.
    const NamedSolid box = occt::makeNamedBox(20.0, 10.0, 40.0, {-10.0, -5.0, 0.0}, 0.0);
    const NamedSolid lying = occt::rotateNamed(box, {0.0, 0.0, 20.0}, {90.0, 0.0, 0.0});
    const SolidReport report = occt::measure(lying.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(8000.0));
    CHECK(report.area == exactly(2.0 * (20.0 * 10.0 + 20.0 * 40.0 + 10.0 * 40.0)));
    CHECK(report.boxMin.x == Approx(-10.0).margin(1e-9));
    CHECK(report.boxMax.x == Approx(10.0).margin(1e-9));
    CHECK(report.boxMin.y == Approx(-20.0).margin(1e-9));
    CHECK(report.boxMax.y == Approx(20.0).margin(1e-9));
    CHECK(report.boxMin.z == Approx(15.0).margin(1e-9));
    CHECK(report.boxMax.z == Approx(25.0).margin(1e-9));
    CHECK(report.faces == 6);
    CHECK(report.edges == 12);
    CHECK(report.vertices == 8);
}

TEST_CASE("kernel turn: no turn at all gives the body back unchanged", "[solid]") {
    const NamedSolid box = occt::makeNamedBox(20.0, 10.0, 40.0, {}, 0.0);
    const NamedSolid same = occt::rotateNamed(box, {1.0, 2.0, 3.0}, {});
    CHECK(same.solid.data() == box.solid.data());
}

TEST_CASE("kernel turn: the faces keep their names, 'top' is now wherever the top went",
          "[solid][golden]") {
    const NamedSolid box = occt::makeNamedBox(20.0, 10.0, 40.0, {-10.0, -5.0, 0.0}, 0.0);
    const Angles angles{90.0, 0.0, 90.0};
    const NamedSolid turned = occt::rotateNamed(box, {0.0, 0.0, 20.0}, angles);

    REQUIRE(turned.faces.size() == box.faces.size());
    for (std::size_t i = 0; i < box.faces.size(); ++i) {
        CHECK(turned.faces[i].faceId == box.faces[i].faceId);
        CHECK(turned.faces[i].role == box.faces[i].role);
    }

    // Each named face of the turned body faces where the same face of the first body, turned,
    // faces.
    const TriangleMesh before = occt::tessellate(box.solid);
    const TriangleMesh after = occt::tessellate(turned.solid);
    for (const NamedFace& face : box.faces) {
        const Point3d wanted = rotatedDirection(averageNormal(before, face.faceId), angles);
        const Point3d got = averageNormal(after, face.faceId);
        CAPTURE(face.role);
        CHECK(got.x == Approx(wanted.x).margin(1e-6));
        CHECK(got.y == Approx(wanted.y).margin(1e-6));
        CHECK(got.z == Approx(wanted.z).margin(1e-6));
    }
}

TEST_CASE("kernel turn: round things stay round", "[solid][golden]") {
    // A box with 5 mm corners and a cylinder, tipped by an odd angle.
    const NamedSolid rounded = occt::makeNamedBox(40.0, 30.0, 10.0, {}, 5.0);
    const SolidReport before = occt::measure(rounded.solid);
    const NamedSolid turned = occt::rotateNamed(rounded, {20.0, 15.0, 5.0}, {17.0, 29.0, 61.0});
    const SolidReport after = occt::measure(turned.solid);
    CHECK(after.valid);
    CHECK(after.volume == exactly(before.volume));
    CHECK(after.area == exactly(before.area));
    CHECK(after.faces == before.faces);

    const NamedSolid cylinder = occt::makeNamedCylinder(8.0, 30.0, {});
    const NamedSolid tipped = occt::rotateNamed(cylinder, {0, 0, 15}, {90.0, 0.0, 0.0});
    const SolidReport side = occt::measure(tipped.solid);
    CHECK(side.valid);
    CHECK(side.volume == exactly(std::numbers::pi * 64.0 * 30.0));
    CHECK(side.faces == 3);
}

TEST_CASE("kernel turn: the rest of the kernel understands a turned body", "[solid]") {
    const NamedSolid sphere = occt::makeNamedPrimitive(SphereSpec{10.0}, {0, 0, 0});
    const NamedSolid turned = occt::rotateNamed(sphere, {0, 0, 10}, {30.0, 40.0, 50.0});
    const TriangleMesh mesh = occt::tessellate(turned.solid);
    CHECK(mesh.triangleCount() > 100);
    const Bounds box = occt::bounds(turned.solid);
    CHECK(box.max.z - box.min.z == Approx(20.0).margin(0.01));
}

TEST_CASE("kernel turn: the exact box touches a turned body", "[solid]") {
    // A cylinder r 8, h 30 tipped 90 degrees about X round its middle (0, 0, 15): it lies along Y.
    const NamedSolid cylinder = occt::makeNamedCylinder(8.0, 30.0, {0, 0, 0});
    const NamedSolid tipped = occt::rotateNamed(cylinder, {0, 0, 15}, {90.0, 0.0, 0.0});
    const Bounds box = occt::exactBounds(tipped.solid);
    CHECK(box.min.x == Approx(-8.0).margin(1e-6));
    CHECK(box.max.x == Approx(8.0).margin(1e-6));
    CHECK(box.min.y == Approx(-15.0).margin(1e-6));
    CHECK(box.max.y == Approx(15.0).margin(1e-6));
    CHECK(box.min.z == Approx(7.0).margin(1e-6));
    CHECK(box.max.z == Approx(23.0).margin(1e-6));
    // Turned at a slant, a sphere still has the box of a sphere.
    const NamedSolid sphere = occt::makeNamedPrimitive(SphereSpec{10.0}, {0, 0, 0});
    const Bounds round =
        occt::exactBounds(occt::rotateNamed(sphere, {0, 0, 10}, {30.0, 40.0, 50.0}).solid);
    CHECK(round.min.z == Approx(0.0).margin(1e-6));
    CHECK(round.max.z == Approx(20.0).margin(1e-6));
    CHECK(round.max.x == Approx(10.0).margin(1e-6));
}

TEST_CASE("kernel turn: values that are not numbers are refused", "[solid]") {
    const NamedSolid box = occt::makeNamedBox(10, 10, 10, {}, 0.0);
    CHECK_THROWS_AS(occt::rotateNamed(box, {0, 0, 0}, {std::nan(""), 0, 1}), GeometryError);
    CHECK_THROWS_AS(occt::rotateNamed(box, {INFINITY, 0, 0}, {1, 0, 0}), GeometryError);
}
