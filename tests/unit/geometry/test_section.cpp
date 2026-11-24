// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The cut face of a body along a plane (AP-2.11a): golden areas from the formulas.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

#include "fission/geo/error.h"
#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kDeflection = 0.01;

double dot(Point3d a, Point3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Area of the triangles, which must all lie in the plane and face the normal.
double triangleArea(const SectionMesh& s, Point3d point, Point3d normal) {
    const double length = std::sqrt(dot(normal, normal));
    normal = {normal.x / length, normal.y / length, normal.z / length};
    double area = 0.0;
    for (const Point3d& p : s.positions) {
        CHECK(std::abs(dot({p.x - point.x, p.y - point.y, p.z - point.z}, normal)) < 1e-7);
    }
    for (std::size_t i = 0; i + 2 < s.indices.size(); i += 3) {
        const Point3d& a = s.positions[s.indices[i]];
        const Point3d& b = s.positions[s.indices[i + 1]];
        const Point3d& c = s.positions[s.indices[i + 2]];
        const Point3d u{b.x - a.x, b.y - a.y, b.z - a.z};
        const Point3d v{c.x - a.x, c.y - a.y, c.z - a.z};
        const Point3d cross{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        CHECK(dot(cross, normal) > 0.0);
        area += 0.5 * dot(cross, normal);
    }
    return area;
}

// 40 x 30 x 20 box with a hole of diameter 10 along Z through (20, 15).
NamedSolid holedBox(const GeometryKernel& kernel) {
    const NamedSolid box = kernel.makeBox(40, 30, 20, {0, 0, 0}, 0.0);
    const NamedSolid pin = kernel.makeCylinder(5.0, 40.0, {20, 15, -10});
    return kernel.boolean(BooleanOp::subtract, box, {pin});
}

} // namespace

TEST_CASE("section: across a hole the face is the rectangle minus the circle",
          "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid body = holedBox(*kernel);
    const Point3d point{20, 15, 10};
    const Point3d normal{0, 0, 1};
    const SectionMesh s = kernel->sectionMesh(body.solid, point, normal, kDeflection);
    REQUIRE_FALSE(s.empty());
    CHECK(s.area == Approx(40.0 * 30.0 - kPi * 25.0).epsilon(1e-6));
    CHECK(triangleArea(s, point, normal) == Approx(s.area).epsilon(1e-3));
    // The other way round gives the same face, facing the other way.
    const SectionMesh back = kernel->sectionMesh(body.solid, point, {0, 0, -1}, kDeflection);
    CHECK(back.area == Approx(s.area).epsilon(1e-9));
    CHECK(triangleArea(back, point, {0, 0, -1}) == Approx(s.area).epsilon(1e-3));
}

TEST_CASE("section: along a hole the face is the rectangle minus a strip", "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid body = holedBox(*kernel);
    const Point3d point{20, 15, 10};
    const Point3d normal{0, 1, 0};
    const SectionMesh s = kernel->sectionMesh(body.solid, point, normal, kDeflection);
    CHECK(s.area == Approx(40.0 * 20.0 - 10.0 * 20.0).epsilon(1e-6));
    CHECK(triangleArea(s, point, normal) == Approx(s.area).epsilon(1e-6));
}

TEST_CASE("section: a hollow body gives a ring", "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid tube = kernel->makePrimitive(TubeSpec{10.0, 2.0, 20.0}, {0, 0, 0});
    const Point3d point{0, 0, 10};
    const SectionMesh s = kernel->sectionMesh(tube.solid, point, {0, 0, 1}, kDeflection);
    CHECK(s.area == Approx(kPi * (100.0 - 64.0)).epsilon(1e-6));
    CHECK(triangleArea(s, point, {0, 0, 1}) == Approx(s.area).epsilon(1e-3));
}

TEST_CASE("section: two separate pieces are both there", "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid base = kernel->makeBox(40, 10, 10, {0, 0, 0}, 0.0);
    const NamedSolid left = kernel->makeBox(10, 10, 20, {0, 0, 10}, 0.0);
    const NamedSolid right = kernel->makeBox(10, 10, 20, {30, 0, 10}, 0.0);
    const NamedSolid u = kernel->boolean(BooleanOp::merge, base, {left, right});
    const Point3d point{0, 0, 20};
    const SectionMesh s = kernel->sectionMesh(u.solid, point, {0, 0, 1}, kDeflection);
    CHECK(s.area == Approx(200.0).epsilon(1e-6));
    CHECK(triangleArea(s, point, {0, 0, 1}) == Approx(200.0).epsilon(1e-6));
}

TEST_CASE("section: a slanted plane through a cube is a rectangle with the diagonal",
          "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid cube = kernel->makeBox(10, 10, 10, {0, 0, 0}, 0.0);
    const Point3d point{5, 5, 5};
    const Point3d normal{1, -1, 0};
    const SectionMesh s = kernel->sectionMesh(cube.solid, point, normal, kDeflection);
    CHECK(s.area == Approx(10.0 * 10.0 * std::sqrt(2.0)).epsilon(1e-6));
    CHECK(triangleArea(s, point, normal) == Approx(s.area).epsilon(1e-6));
}

TEST_CASE("section: a plane that misses or only touches gives nothing, never a crash",
          "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid cube = kernel->makeBox(10, 10, 10, {0, 0, 0}, 0.0);
    CHECK(kernel->sectionMesh(cube.solid, {0, 0, 50}, {0, 0, 1}, kDeflection).empty());
    const SectionMesh onFace = kernel->sectionMesh(cube.solid, {0, 0, 10}, {0, 0, 1}, kDeflection);
    CHECK(onFace.area < 1e-6 * 100.0 + 100.0); // empty or the face itself: both are allowed
    const SectionMesh onBottom = kernel->sectionMesh(cube.solid, {0, 0, 0}, {0, 0, 1}, kDeflection);
    CHECK(onBottom.area < 100.0 + 1e-6);
}

TEST_CASE("section: through the middle of a sphere the face is a disc", "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid ball = kernel->makePrimitive(SphereSpec{10.0}, {0, 0, 0});
    const SolidReport report = kernel->measure(ball.solid);
    const Point3d centre{(report.boxMin.x + report.boxMax.x) / 2,
                         (report.boxMin.y + report.boxMax.y) / 2,
                         (report.boxMin.z + report.boxMax.z) / 2};
    const SectionMesh s = kernel->sectionMesh(ball.solid, centre, {0, 0, 1}, kDeflection);
    CHECK(s.area == Approx(kPi * 100.0).epsilon(1e-6));
    // The polygon lies inside the circle: its area is smaller, within the tolerance.
    const double triangles = triangleArea(s, centre, {0, 0, 1});
    CHECK(triangles <= s.area + 1e-9);
    CHECK(triangles == Approx(s.area).epsilon(1e-3));
}

TEST_CASE("section: nonsense input is refused", "[geometry][section]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid cube = kernel->makeBox(10, 10, 10, {0, 0, 0}, 0.0);
    const auto code = [&](Point3d point, Point3d normal, double deflection) {
        try {
            kernel->sectionMesh(cube.solid, point, normal, deflection);
        } catch (const GeometryError& error) {
            return error.code();
        }
        return GeometryErrorCode::kernelFailure;
    };
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(code({0, 0, 5}, {0, 0, 0}, 0.01) == GeometryErrorCode::invalidInput);
    CHECK(code({0, 0, 5}, {0, nan, 1}, 0.01) == GeometryErrorCode::invalidInput);
    CHECK(code({nan, 0, 5}, {0, 0, 1}, 0.01) == GeometryErrorCode::invalidInput);
    CHECK(code({0, 0, 5}, {0, 0, 1}, 0.0) == GeometryErrorCode::invalidInput);
}
