// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Datum planes (AP-2.12 part 1): pure maths on Point3d, no kernel — offset along the normal,
// through three points, halfway between two parallel planes, turned about an edge, tangent to a
// cylinder — and the requests that are refused.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "fission/geo/datum.h"
#include "fission/geo/error.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

const DatumPlane kXY{{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}};

double dot(Point3d a, Point3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(Point3d v) {
    return std::sqrt(dot(v, v));
}

void checkPoint(Point3d p, double x, double y, double z) {
    CHECK(p.x == Approx(x).margin(1e-9));
    CHECK(p.y == Approx(y).margin(1e-9));
    CHECK(p.z == Approx(z).margin(1e-9));
}

/// The frame of a datum plane: unit normal and u axis, perpendicular to each other.
void checkFrame(const DatumPlane& plane) {
    CHECK(length(plane.normal) == Approx(1.0).margin(1e-12));
    CHECK(length(plane.uAxis) == Approx(1.0).margin(1e-12));
    CHECK(dot(plane.normal, plane.uAxis) == Approx(0.0).margin(1e-12));
}

template <typename F> void checkInvalidInput(F&& f, const char* subject) {
    try {
        f();
        FAIL("expected a GeometryError");
    } catch (const GeometryError& e) {
        CHECK(e.code() == GeometryErrorCode::invalidInput);
        CHECK(e.subject() == subject);
    }
}

} // namespace

TEST_CASE("datum: offsetPlane moves along the normal, keeps normal and u axis",
          "[geometry][datum]") {
    const DatumPlane up = offsetPlane(kXY, 10.0);
    checkPoint(up.origin, 0.0, 0.0, 10.0);
    checkPoint(up.normal, 0.0, 0.0, 1.0);
    checkPoint(up.uAxis, 1.0, 0.0, 0.0);
    checkFrame(up);

    const DatumPlane down = offsetPlane(kXY, -4.0);
    checkPoint(down.origin, 0.0, 0.0, -4.0);
    checkFrame(down);

    CHECK(offsetPlane(kXY, 0.0) == kXY);
}

TEST_CASE("datum: planeThroughPoints builds the plane of three points", "[geometry][datum]") {
    const DatumPlane p = planeThroughPoints({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
    checkPoint(p.origin, 0.0, 0.0, 0.0);
    checkPoint(p.uAxis, 1.0, 0.0, 0.0);
    checkPoint(p.normal, 0.0, 0.0, 1.0);
    checkFrame(p);

    // The other way round flips the normal.
    const DatumPlane q = planeThroughPoints({0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0});
    checkPoint(q.normal, 0.0, 0.0, -1.0);
    checkFrame(q);

    checkInvalidInput([] { planeThroughPoints({0, 0, 0}, {1, 1, 1}, {2, 2, 2}); },
                      "datumDegenerate"); // on one line
    checkInvalidInput([] { planeThroughPoints({0, 0, 0}, {1, 0, 0}, {0, std::nan(""), 0}); },
                      "datumDegenerate");
}

TEST_CASE("datum: midPlane halves the distance of parallel planes", "[geometry][datum]") {
    const DatumPlane z10{{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}};
    const DatumPlane mid = midPlane(kXY, z10);
    checkPoint(mid.origin, 0.0, 0.0, 5.0);
    checkPoint(mid.normal, 0.0, 0.0, 1.0);
    checkPoint(mid.uAxis, 1.0, 0.0, 0.0);
    checkFrame(mid);

    // An opposite normal is still parallel.
    const DatumPlane z10Flipped{{0.0, 0.0, 10.0}, {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}};
    checkPoint(midPlane(kXY, z10Flipped).origin, 0.0, 0.0, 5.0);

    // Coincident planes give that plane.
    CHECK(midPlane(kXY, kXY) == kXY);

    const DatumPlane xz{{0.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}};
    checkInvalidInput([&] { midPlane(kXY, xz); }, "datumNotParallel");
}

TEST_CASE("datum: anglePlane turns the reference about an edge in it", "[geometry][datum]") {
    // Rodrigues: (0,0,1) turned 90° right-handed about +x gives (0,-1,0).
    const DatumPlane p90 = anglePlane({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, kXY, 90.0);
    checkPoint(p90.origin, 0.0, 0.0, 0.0);
    checkPoint(p90.uAxis, 1.0, 0.0, 0.0);
    checkPoint(p90.normal, 0.0, -1.0, 0.0);
    checkFrame(p90);

    // 45° about +x: (0,0,1) → (0, -sin45, cos45).
    const DatumPlane p45 = anglePlane({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, kXY, 45.0);
    checkPoint(p45.normal, 0.0, -std::sqrt(0.5), std::sqrt(0.5));
    checkFrame(p45);

    // 0° keeps the reference normal.
    const DatumPlane p0 = anglePlane({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, kXY, 0.0);
    checkPoint(p0.normal, 0.0, 0.0, 1.0);
    checkFrame(p0);

    // The edge must lie in the reference plane.
    checkInvalidInput([] { anglePlane({0, 0, 1}, {1, 0, 0}, kXY, 45.0); }, "datumEdgeNotInPlane");
    checkInvalidInput([] { anglePlane({0, 0, 0}, {1, 0, 1}, kXY, 45.0); }, "datumEdgeNotInPlane");
    checkInvalidInput([] { anglePlane({0, 0, 0}, {0, 0, 0}, kXY, 45.0); }, "datumDegenerate");
}

TEST_CASE("datum: tangentPlane touches the cylinder along a line", "[geometry][datum]") {
    const DatumPlane t = tangentPlane({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 5.0, {1.0, 0.0, 0.0});
    checkPoint(t.origin, 5.0, 0.0, 0.0);
    checkPoint(t.normal, 1.0, 0.0, 0.0);
    checkPoint(t.uAxis, 0.0, 0.0, 1.0);
    checkFrame(t);

    // A radial direction with an axial part is projected first; a longer axis changes nothing.
    const DatumPlane t2 = tangentPlane({0.0, 0.0, 0.0}, {0.0, 0.0, 2.0}, 5.0, {1.0, 0.0, 3.0});
    checkPoint(t2.origin, 5.0, 0.0, 0.0);
    checkPoint(t2.normal, 1.0, 0.0, 0.0);
    checkPoint(t2.uAxis, 0.0, 0.0, 1.0);
    checkFrame(t2);

    checkInvalidInput([] { tangentPlane({0, 0, 0}, {0, 0, 1}, 0.0, {1, 0, 0}); },
                      "datumDegenerate"); // radius zero
    checkInvalidInput([] { tangentPlane({0, 0, 0}, {0, 0, 1}, -5.0, {1, 0, 0}); },
                      "datumDegenerate"); // radius negative
    checkInvalidInput([] { tangentPlane({0, 0, 0}, {0, 0, 1}, 5.0, {0, 0, 2}); },
                      "datumDegenerate"); // radial parallel to the axis
    checkInvalidInput([] { tangentPlane({0, 0, 0}, {0, 0, 0}, 5.0, {1, 0, 0}); },
                      "datumDegenerate"); // zero axis
}
