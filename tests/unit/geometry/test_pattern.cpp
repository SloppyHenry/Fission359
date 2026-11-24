// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/pattern.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

// Helper to check if a matrix is identity
bool isIdentityMatrix(const double r[3][3]) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (std::abs(r[i][j] - (i == j ? 1.0 : 0.0)) > 1e-12) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Transform3 basic tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: Transform3 default constructor creates identity", "[geometry][pattern]") {
    Transform3 t;
    CHECK(isIdentityMatrix(t.r));
    CHECK(t.t.x == Approx(0.0).margin(1e-12));
    CHECK(t.t.y == Approx(0.0).margin(1e-12));
    CHECK(t.t.z == Approx(0.0).margin(1e-12));
}

TEST_CASE("pattern: apply identity to point returns same point", "[geometry][pattern]") {
    Transform3 t;
    Point3d p = {1.0, 2.0, 3.0};
    Point3d result = apply(t, p);
    CHECK(result.x == Approx(1.0).margin(1e-12));
    CHECK(result.y == Approx(2.0).margin(1e-12));
    CHECK(result.z == Approx(3.0).margin(1e-12));
}

TEST_CASE("pattern: applyToDirection ignores translation", "[geometry][pattern]") {
    Transform3 t;
    t.t = {10.0, 20.0, 30.0};
    Point3d v = {1.0, 0.0, 0.0};
    Point3d result = applyToDirection(t, v);
    CHECK(result.x == Approx(1.0).margin(1e-12));
    CHECK(result.y == Approx(0.0).margin(1e-12));
    CHECK(result.z == Approx(0.0).margin(1e-12));
}

TEST_CASE("pattern: determinant of identity is 1", "[geometry][pattern]") {
    Transform3 t;
    CHECK(determinant(t) == Approx(1.0).margin(1e-12));
}

// ---------------------------------------------------------------------------
// linearPattern tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: linearPattern x-direction spacing 15 count 4", "[geometry][pattern]") {
    Point3d direction = {1.0, 0.0, 0.0};
    auto transforms = linearPattern(direction, 15.0, 4);

    CHECK(transforms.size() == 4);

    // Check each transformation
    for (size_t i = 0; i < transforms.size(); ++i) {
        CHECK(isIdentityMatrix(transforms[i].r));
        CHECK(transforms[i].t.x == Approx(i * 15.0).margin(1e-9));
        CHECK(transforms[i].t.y == Approx(0.0).margin(1e-12));
        CHECK(transforms[i].t.z == Approx(0.0).margin(1e-12));
    }
}

TEST_CASE("pattern: linearPattern with non-unit direction (3,0,4) is normalized",
          "[geometry][pattern]") {
    Point3d direction = {3.0, 0.0, 4.0};
    auto transforms = linearPattern(direction, 5.0, 2);

    CHECK(transforms.size() == 2);

    // Instance 0: identity
    CHECK(transforms[0].t.x == Approx(0.0).margin(1e-12));

    // Instance 1: translation by spacing along normalized direction
    // normalized direction = (3/5, 0, 4/5)
    // translation = 5 * (3/5, 0, 4/5) = (3, 0, 4)
    CHECK(transforms[1].t.x == Approx(3.0).margin(1e-9));
    CHECK(transforms[1].t.y == Approx(0.0).margin(1e-12));
    CHECK(transforms[1].t.z == Approx(4.0).margin(1e-9));
}

TEST_CASE("pattern: linearPattern count 0 throws patternCount", "[geometry][pattern]") {
    try {
        (void)linearPattern({1, 0, 0}, 10.0, 0);
        FAIL("Expected GeometryError with subject patternCount");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternCount");
    }
}

TEST_CASE("pattern: linearPattern count 10001 throws patternCount", "[geometry][pattern]") {
    try {
        (void)linearPattern({1, 0, 0}, 10.0, 10001);
        FAIL("Expected GeometryError with subject patternCount");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternCount");
    }
}

TEST_CASE("pattern: linearPattern zero direction throws patternDegenerate", "[geometry][pattern]") {
    try {
        (void)linearPattern({0, 0, 0}, 10.0, 5);
        FAIL("Expected GeometryError with subject patternDegenerate");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternDegenerate");
    }
}

TEST_CASE("pattern: linearPattern allows zero and negative spacing", "[geometry][pattern]") {
    Point3d direction = {1.0, 0.0, 0.0};
    auto transformsZero = linearPattern(direction, 0.0, 3);
    CHECK(transformsZero.size() == 3);
    CHECK(transformsZero[0].t.x == Approx(0.0).margin(1e-12));
    CHECK(transformsZero[1].t.x == Approx(0.0).margin(1e-12));
    CHECK(transformsZero[2].t.x == Approx(0.0).margin(1e-12));

    auto transformsNeg = linearPattern(direction, -10.0, 3);
    CHECK(transformsNeg.size() == 3);
    CHECK(transformsNeg[0].t.x == Approx(0.0).margin(1e-12));
    CHECK(transformsNeg[1].t.x == Approx(-10.0).margin(1e-12));
    CHECK(transformsNeg[2].t.x == Approx(-20.0).margin(1e-12));
}

// ---------------------------------------------------------------------------
// linearPatternOverLength tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: linearPatternOverLength total 90 count 4", "[geometry][pattern]") {
    Point3d direction = {1.0, 0.0, 0.0};
    auto transforms = linearPatternOverLength(direction, 90.0, 4);

    CHECK(transforms.size() == 4);

    // Spacing should be 90 / (4 - 1) = 30
    for (size_t i = 0; i < transforms.size(); ++i) {
        CHECK(transforms[i].t.x == Approx(i * 30.0).margin(1e-9));
    }
}

TEST_CASE("pattern: linearPatternOverLength count 1 throws patternCount", "[geometry][pattern]") {
    try {
        (void)linearPatternOverLength({1, 0, 0}, 90.0, 1);
        FAIL("Expected GeometryError with subject patternCount");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternCount");
    }
}

// ---------------------------------------------------------------------------
// gridPattern tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: gridPattern 3x4 creates 12 transformations", "[geometry][pattern]") {
    Point3d dirA = {1.0, 0.0, 0.0};
    Point3d dirB = {0.0, 1.0, 0.0};
    auto transforms = gridPattern(dirA, 10.0, 3, dirB, 15.0, 4);

    CHECK(transforms.size() == 12);

    // Index 0: i=0, j=0 -> t = (0, 0, 0)
    CHECK(transforms[0].t.x == Approx(0.0).margin(1e-12));
    CHECK(transforms[0].t.y == Approx(0.0).margin(1e-12));

    // Index 5: i=2, j=1 -> t = (2*10, 1*15, 0) = (20, 15, 0)
    CHECK(transforms[5].t.x == Approx(20.0).margin(1e-9));
    CHECK(transforms[5].t.y == Approx(15.0).margin(1e-9));
    CHECK(transforms[5].t.z == Approx(0.0).margin(1e-12));
}

TEST_CASE("pattern: gridPattern total count exceeds 10000 throws patternCount",
          "[geometry][pattern]") {
    try {
        // 101 * 100 = 10100 > 10000
        (void)gridPattern({1, 0, 0}, 1.0, 101, {0, 1, 0}, 1.0, 100);
        FAIL("Expected GeometryError with subject patternCount");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternCount");
    }
}

// ---------------------------------------------------------------------------
// circularPattern tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: circularPattern around z-axis count 6 360 degrees", "[geometry][pattern]") {
    Point3d axisPoint = {0.0, 0.0, 0.0};
    Point3d axisDirection = {0.0, 0.0, 1.0};
    auto transforms = circularPattern(axisPoint, axisDirection, 6, 360.0);

    CHECK(transforms.size() == 6);

    // Instance 0: identity
    CHECK(isIdentityMatrix(transforms[0].r));
    CHECK(transforms[0].t.x == Approx(0.0).margin(1e-12));

    // Instance 1: rotation by 60 degrees around z-axis
    // (10, 0, 0) rotated by 60 degrees -> (10*cos(60), 10*sin(60), 0) = (5, 8.660254..., 0)
    Point3d p = {10.0, 0.0, 0.0};
    Point3d result1 = apply(transforms[1], p);
    CHECK(result1.x == Approx(5.0).margin(1e-9));
    CHECK(result1.y == Approx(8.660254037844386).margin(1e-9));
    CHECK(result1.z == Approx(0.0).margin(1e-12));

    // Instance 3: rotation by 180 degrees around z-axis
    // (10, 0, 0) rotated by 180 degrees -> (-10, 0, 0)
    Point3d result3 = apply(transforms[3], p);
    CHECK(result3.x == Approx(-10.0).margin(1e-9));
    CHECK(result3.y == Approx(0.0).margin(1e-12));
    CHECK(result3.z == Approx(0.0).margin(1e-12));
}

TEST_CASE("pattern: circularPattern with offset axis point", "[geometry][pattern]") {
    Point3d axisPoint = {10.0, 0.0, 0.0};
    Point3d axisDirection = {0.0, 0.0, 1.0};
    auto transforms = circularPattern(axisPoint, axisDirection, 6, 360.0);

    // The point (10, 0, 0) should remain fixed for all transformations
    Point3d p = {10.0, 0.0, 0.0};
    for (const auto& t : transforms) {
        Point3d result = apply(t, p);
        CHECK(result.x == Approx(10.0).margin(1e-9));
        CHECK(result.y == Approx(0.0).margin(1e-9));
        CHECK(result.z == Approx(0.0).margin(1e-9));
    }
}

TEST_CASE("pattern: circularPattern 180 degrees count 3", "[geometry][pattern]") {
    Point3d axisPoint = {0.0, 0.0, 0.0};
    Point3d axisDirection = {0.0, 0.0, 1.0};
    auto transforms = circularPattern(axisPoint, axisDirection, 3, 180.0);

    CHECK(transforms.size() == 3);

    // Angular step should be 180 / (3 - 1) = 90 degrees
    // Instance 1: 90 degrees, Instance 2: 180 degrees
    Point3d p = {1.0, 0.0, 0.0};
    Point3d result1 = apply(transforms[1], p);
    CHECK(result1.x == Approx(0.0).margin(1e-9));
    CHECK(result1.y == Approx(1.0).margin(1e-9));

    Point3d result2 = apply(transforms[2], p);
    CHECK(result2.x == Approx(-1.0).margin(1e-9));
    CHECK(result2.y == Approx(0.0).margin(1e-9));
}

TEST_CASE("pattern: circularPattern zero axis direction throws patternDegenerate",
          "[geometry][pattern]") {
    try {
        (void)circularPattern({0, 0, 0}, {0, 0, 0}, 6, 360.0);
        FAIL("Expected GeometryError with subject patternDegenerate");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternDegenerate");
    }
}

// ---------------------------------------------------------------------------
// mirrorPlane tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: mirrorPlane at x=5 reflects point (7,2,3) to (3,2,3)", "[geometry][pattern]") {
    Point3d planePoint = {5.0, 0.0, 0.0};
    Point3d planeNormal = {1.0, 0.0, 0.0};
    Transform3 mirror = mirrorPlane(planePoint, planeNormal);

    // Check determinant is -1 (reflection)
    CHECK(determinant(mirror) == Approx(-1.0).margin(1e-12));

    // Reflect point (7, 2, 3): distance from plane is 2, so reflection moves it 4 units left
    Point3d p = {7.0, 2.0, 3.0};
    Point3d result = apply(mirror, p);
    CHECK(result.x == Approx(3.0).margin(1e-9));
    CHECK(result.y == Approx(2.0).margin(1e-12));
    CHECK(result.z == Approx(3.0).margin(1e-12));

    // Check direction reflection
    Point3d dir = {1.0, 0.0, 0.0};
    Point3d resultDir = applyToDirection(mirror, dir);
    CHECK(resultDir.x == Approx(-1.0).margin(1e-12));
    CHECK(resultDir.y == Approx(0.0).margin(1e-12));
    CHECK(resultDir.z == Approx(0.0).margin(1e-12));
}

TEST_CASE("pattern: mirrorPlane zero normal throws patternDegenerate", "[geometry][pattern]") {
    try {
        (void)mirrorPlane({0, 0, 0}, {0, 0, 0});
        FAIL("Expected GeometryError with subject patternDegenerate");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::invalidInput);
        CHECK(error.subject() == "patternDegenerate");
    }
}

// ---------------------------------------------------------------------------
// withSkipped tests
// ---------------------------------------------------------------------------

TEST_CASE("pattern: withSkipped removes specified indices", "[geometry][pattern]") {
    // Create 4 transformations
    Point3d direction = {1.0, 0.0, 0.0};
    auto all = linearPattern(direction, 10.0, 4);

    // Skip indices 0, 2, 9 (0 is ignored, 9 is out of range)
    auto result = withSkipped(all, {0, 2, 9});

    // Should have 3 transformations (index 2 removed)
    CHECK(result.size() == 3);

    // Check that index 2 is missing
    CHECK(result[0].t.x == Approx(0.0).margin(1e-12));  // original index 0
    CHECK(result[1].t.x == Approx(10.0).margin(1e-12)); // original index 1
    CHECK(result[2].t.x == Approx(30.0).margin(1e-12)); // original index 3 (2 skipped)
}

TEST_CASE("pattern: withSkipped cannot skip index 0", "[geometry][pattern]") {
    Point3d direction = {1.0, 0.0, 0.0};
    auto all = linearPattern(direction, 10.0, 4);

    // Try to skip index 0
    auto result = withSkipped(all, {0});

    // Index 0 is not skipped, so all 4 remain
    CHECK(result.size() == 4);
}

TEST_CASE("pattern: withSkipped ignores out of range indices", "[geometry][pattern]") {
    Point3d direction = {1.0, 0.0, 0.0};
    auto all = linearPattern(direction, 10.0, 4);

    // Skip indices that are out of range
    auto result = withSkipped(all, {10, -1, 100});

    // All 4 should remain
    CHECK(result.size() == 4);
}
