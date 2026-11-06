// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Measuring corners, edges and faces (AP-2.11): golden values against the formulas.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

// The number of the face with this role (its role ends with `ending`).
std::uint32_t face(const NamedSolid& solid, const std::string& ending) {
    for (const NamedFace& f : solid.faces) {
        if (f.role.size() >= ending.size() &&
            f.role.compare(f.role.size() - ending.size(), ending.size(), ending) == 0) {
            return f.faceId;
        }
    }
    FAIL("no face " << ending);
    return 0;
}

ElementRef faceOf(const NamedSolid& solid, const std::string& role) {
    return {solid.solid, ElementKind::face, face(solid, role)};
}

// The edge of the body whose two ends are the given corners (positions), found through the mesh.
ElementRef edgeBetween(const GeometryKernel& kernel, const NamedSolid& solid, Point3d a,
                       Point3d b) {
    const TriangleMesh mesh = kernel.tessellate(solid.solid, {});
    const auto near = [](const Point3f& p, const Point3d& q) {
        return std::abs(p.x - q.x) < 1e-4 && std::abs(p.y - q.y) < 1e-4 &&
               std::abs(p.z - q.z) < 1e-4;
    };
    for (const EdgePolyline& edge : mesh.edges) {
        const Point3f& first = edge.points.front();
        const Point3f& last = edge.points.back();
        if ((near(first, a) && near(last, b)) || (near(first, b) && near(last, a))) {
            return {solid.solid, ElementKind::edge, edge.edgeId};
        }
    }
    FAIL("no such edge");
    return {};
}

ElementRef cornerAt(const GeometryKernel& kernel, const NamedSolid& solid, Point3d at) {
    const TriangleMesh mesh = kernel.tessellate(solid.solid, {});
    for (const CornerPoint& corner : mesh.corners) {
        if (std::abs(corner.position.x - at.x) < 1e-4 &&
            std::abs(corner.position.y - at.y) < 1e-4 &&
            std::abs(corner.position.z - at.z) < 1e-4) {
            return {solid.solid, ElementKind::corner, corner.cornerId};
        }
    }
    FAIL("no such corner");
    return {};
}

} // namespace

TEST_CASE("measure: the distance of two parallel faces of a box", "[measure]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(30.0, 20.0, 12.5, {0, 0, 0});
    const Measurement m = kernel->measureElements({faceOf(box, "left"), faceOf(box, "right")});
    REQUIRE(m.distance);
    CHECK(*m.distance == exactly(30.0));
    REQUIRE(m.angle);
    CHECK(*m.angle == Approx(0.0).margin(1e-9)); // parallel planes
    const Measurement vertical =
        kernel->measureElements({faceOf(box, "bottom"), faceOf(box, "top")});
    CHECK(*vertical.distance == exactly(12.5));
    REQUIRE(m.span);
    CHECK(std::abs(m.span->at(0).x - m.span->at(1).x) == exactly(30.0)); // the line is across
}

TEST_CASE("measure: two faces of different bodies", "[measure]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid a = occt::makeNamedBox(10.0, 10.0, 10.0, {0, 0, 0});
    const NamedSolid b = occt::makeNamedBox(10.0, 10.0, 10.0, {25.0, 0, 0});
    const Measurement m = kernel->measureElements({faceOf(a, "right"), faceOf(b, "left")});
    CHECK(*m.distance == exactly(15.0));
    const Measurement touching = kernel->measureElements(
        {faceOf(a, "right"), faceOf(occt::makeNamedBox(10, 10, 10, {10, 0, 0}), "left")});
    CHECK(*touching.distance == Approx(0.0).margin(1e-9));
}

TEST_CASE("measure: the angle inside a box corner is 90 degrees", "[measure]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(10.0, 20.0, 30.0, {0, 0, 0});
    const Measurement m = kernel->measureElements({faceOf(box, "top"), faceOf(box, "front")});
    REQUIRE(m.angle);
    CHECK(*m.angle == exactly(90.0));
    CHECK(*m.distance == Approx(0.0).margin(1e-9)); // they share an edge
}

TEST_CASE("measure: the angles of a wedge", "[measure]") {
    const auto kernel = occt::makeKernel();
    // A wedge 20 wide, 20 deep, 20 high: its slope rises 20 over 20, so 45 degrees.
    const NamedSolid wedge =
        occt::makeNamedPrimitive(PrimitiveSpec{WedgeSpec{20.0, 20.0, 20.0}}, {0, 0, 0});
    std::vector<std::uint32_t> flat;
    for (const NamedFace& f : wedge.faces) {
        flat.push_back(f.faceId);
    }
    // Between the bottom and the slope (they share an edge) the angle inside is 45 degrees.
    double smallest = 180.0;
    for (const std::uint32_t other : flat) {
        if (other == face(wedge, "bottom")) {
            continue;
        }
        const Measurement m = kernel->measureElements(
            {faceOf(wedge, "bottom"), {wedge.solid, ElementKind::face, other}});
        if (m.angle && *m.distance < 1e-9 && *m.angle > 1e-6) {
            smallest = std::min(smallest, *m.angle);
        }
    }
    CHECK(smallest == exactly(45.0));
}

TEST_CASE("measure: length, radius and area of a cylinder", "[measure]") {
    const auto kernel = occt::makeKernel();
    const double r = 7.5;
    const double h = 20.0;
    const NamedSolid cylinder = occt::makeNamedCylinder(r, h, {0, 0, 0});

    const Measurement top = kernel->measureElements({faceOf(cylinder, "top")});
    REQUIRE(top.area);
    CHECK(*top.area == exactly(kPi * r * r));
    CHECK_FALSE(top.radius); // a flat circle face has no radius of its own, its edge has

    const Measurement side = kernel->measureElements({faceOf(cylinder, "side")});
    CHECK(*side.area == exactly(2.0 * kPi * r * h));
    REQUIRE(side.radius);
    CHECK(*side.radius == exactly(r));

    // The edge round the top: a circle of length 2 pi r and radius r.
    const TriangleMesh mesh = kernel->tessellate(cylinder.solid, {});
    bool foundCircle = false;
    for (const EdgePolyline& edge : mesh.edges) {
        const Measurement m =
            kernel->measureElements({{cylinder.solid, ElementKind::edge, edge.edgeId}});
        if (m.radius) {
            foundCircle = true;
            CHECK(*m.radius == exactly(r));
            REQUIRE(m.length);
            CHECK(*m.length == exactly(2.0 * kPi * r));
        }
    }
    CHECK(foundCircle);
}

TEST_CASE("measure: the length of a straight edge and the distance of two corners", "[measure]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(30.0, 40.0, 50.0, {0, 0, 0});
    const ElementRef edge = edgeBetween(*kernel, box, {0, 0, 0}, {30, 0, 0});
    const Measurement m = kernel->measureElements({edge});
    REQUIRE(m.length);
    CHECK(*m.length == exactly(30.0));
    CHECK_FALSE(m.radius);

    const ElementRef a = cornerAt(*kernel, box, {0, 0, 0});
    const ElementRef b = cornerAt(*kernel, box, {30, 40, 0});
    CHECK(*kernel->measureElements({a, b}).distance == exactly(50.0)); // 3-4-5
    const Measurement position = kernel->measureElements({b});
    REQUIRE(position.position);
    CHECK(position.position->x == exactly(30.0));
    CHECK(position.position->y == exactly(40.0));
}

TEST_CASE("measure: the angle between two edges and between an edge and a face", "[measure]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(30.0, 40.0, 50.0, {0, 0, 0});
    const ElementRef alongX = edgeBetween(*kernel, box, {0, 0, 0}, {30, 0, 0});
    const ElementRef alongY = edgeBetween(*kernel, box, {0, 0, 0}, {0, 40, 0});
    const ElementRef alsoX = edgeBetween(*kernel, box, {0, 40, 0}, {30, 40, 0});
    CHECK(*kernel->measureElements({alongX, alongY}).angle == exactly(90.0));
    CHECK(*kernel->measureElements({alongX, alsoX}).angle == Approx(0.0).margin(1e-9));
    CHECK(*kernel->measureElements({alongX, alsoX}).distance == exactly(40.0));
    // An edge on the floor against the top face: parallel; against the left face: square.
    CHECK(*kernel->measureElements({alongX, faceOf(box, "top")}).angle == Approx(0.0).margin(1e-9));
    CHECK(*kernel->measureElements({alongX, faceOf(box, "left")}).angle == exactly(90.0));
}

TEST_CASE("measure: wrong input is refused", "[measure]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(10.0, 10.0, 10.0);
    CHECK_THROWS_AS(kernel->measureElements({}), GeometryError);
    CHECK_THROWS_AS(
        kernel->measureElements({faceOf(box, "top"), faceOf(box, "top"), faceOf(box, "top")}),
        GeometryError);
    CHECK_THROWS_AS(kernel->measureElements({{box.solid, ElementKind::face, 0}}), GeometryError);
    CHECK_THROWS_AS(kernel->measureElements({{box.solid, ElementKind::face, 99}}), GeometryError);
    CHECK_THROWS_AS(kernel->measureElements({{box.solid, ElementKind::edge, 99}}), GeometryError);
    CHECK_THROWS_AS(kernel->measureElements({{Solid{}, ElementKind::face, 1}}), GeometryError);
}
