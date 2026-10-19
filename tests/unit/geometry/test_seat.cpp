// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Putting a shape down on a face (AP-1.6b3): the maths of the turn and of the foot point.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "fission/geo/seat.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

// Where the point `local` of the shape (relative to its foot) ends up once it is seated.
Point3d placed(const Seat& seat, const Point3d& centreOffset, const Point3d& local) {
    const Point3d centre{seat.foot.x + centreOffset.x, seat.foot.y + centreOffset.y,
                         seat.foot.z + centreOffset.z};
    const Point3d at{seat.foot.x + local.x, seat.foot.y + local.y, seat.foot.z + local.z};
    return rotatedAbout(at, centre, seat.turn);
}

// The foot is given in micrometres, so places agree to about a thousandth of a millimetre.
Approx near(double expected) {
    return Approx(expected).margin(2e-3);
}

} // namespace

TEST_CASE("seat: a face looking up gives no turn", "[geometry][seat]") {
    const Seat seat = seatOnFace({10, 20, 5}, {0, 0, 1}, {0, 0, 5});
    CHECK(seat.turn.isZero());
    CHECK(seat.foot.x == near(10));
    CHECK(seat.foot.y == near(20));
    CHECK(seat.foot.z == near(5));
}

TEST_CASE("seat: on a wall the underside touches it and the shape stands out of it",
          "[geometry][seat]") {
    // A shape 10 mm high; its middle is 5 mm above the foot.
    const Point3d offset{0, 0, 5};
    struct Wall {
        Point3d normal;
        Point3d out; // where the top of the shape has to point
    };
    const Wall walls[] = {{{0, -1, 0}, {0, -1, 0}},
                          {{0, 1, 0}, {0, 1, 0}},
                          {{1, 0, 0}, {1, 0, 0}},
                          {{-1, 0, 0}, {-1, 0, 0}}};
    for (const Wall& wall : walls) {
        const Point3d point{3, 4, 12};
        const Seat seat = seatOnFace(point, wall.normal, offset);
        // The middle of the underside is on the wall point ...
        const Point3d foot = placed(seat, offset, {0, 0, 0});
        CHECK(foot.x == near(point.x));
        CHECK(foot.y == near(point.y));
        CHECK(foot.z == near(point.z));
        // ... and the top is 10 mm out of the wall.
        const Point3d top = placed(seat, offset, {0, 0, 10});
        CHECK(top.x == near(point.x + 10 * wall.out.x));
        CHECK(top.y == near(point.y + 10 * wall.out.y));
        CHECK(top.z == near(point.z));
        // The upper edge of the shape points up (the world's Z).
        const Point3d edge = placed(seat, offset, {0, 5, 0});
        CHECK(edge.z == near(point.z + 5));
    }
}

TEST_CASE("seat: a wall facing the viewer reads from left to right", "[geometry][seat]") {
    // The front wall (looking towards -Y): the shape's +X points to the right, to world +X.
    const Point3d offset{0, 0, 0};
    const Seat seat = seatOnFace({0, 0, 0}, {0, -1, 0}, offset);
    const Point3d right = placed(seat, offset, {1, 0, 0});
    CHECK(right.x == near(1));
    CHECK(right.y == near(0));
    CHECK(right.z == near(0));
    // The back wall (looking towards +Y): seen from outside, right is world -X.
    const Seat back = seatOnFace({0, 0, 0}, {0, 1, 0}, offset);
    CHECK(placed(back, offset, {1, 0, 0}).x == near(-1));
}

TEST_CASE("seat: a slope tilts the shape by the slope", "[geometry][seat]") {
    // A face leaning back by 30° from the vertical looks 30° upwards.
    const double a = 30.0 / kDegrees;
    const Point3d normal{0, -std::cos(a), std::sin(a)};
    const Point3d offset{0, 0, 2};
    const Seat seat = seatOnFace({0, 0, 10}, normal, offset);
    const Point3d top = placed(seat, offset, {0, 0, 4});
    CHECK(top.x == near(0));
    CHECK(top.y == near(-4 * std::cos(a)));
    CHECK(top.z == near(10 + 4 * std::sin(a)));
    CHECK(placed(seat, offset, {0, 0, 0}).z == near(10));
}

TEST_CASE("seat: a face looking down turns the shape over", "[geometry][seat]") {
    const Point3d offset{0, 0, 5};
    const Seat seat = seatOnFace({7, 8, 30}, {0, 0, -1}, offset);
    const Point3d foot = placed(seat, offset, {0, 0, 0});
    CHECK(foot.z == near(30));
    CHECK(placed(seat, offset, {0, 0, 10}).z == near(20)); // hangs down
    CHECK(placed(seat, offset, {1, 0, 0}).x == near(8));   // X of the world stays
}

TEST_CASE("seat: angles are whole numbers where the face is straight", "[geometry][seat]") {
    const Seat seat = seatOnFace({0, 0, 0}, {0, -1, 0}, {0, 0, 5});
    CHECK(seat.turn.x == near(90));
    CHECK(seat.turn.y == near(0));
    CHECK(seat.turn.z == near(0));
    // Turned about the middle (5 mm above the foot), the unturned foot lies 5 mm below the wall
    // point and 5 mm in front of it - and without rounding noise.
    CHECK(seat.foot.y == Approx(-5.0).margin(1e-12));
    CHECK(seat.foot.z == Approx(-5.0).margin(1e-12));
}

TEST_CASE("seat: a slope gives readable numbers and still touches the face", "[geometry][seat]") {
    const Point3d normal{0.31, -0.62, 0.72}; // some flank of a curved body
    const Point3d offset{0, 0, 10};
    const Point3d point{12.3456789, -45.6789012, 7.8901234};
    const Seat seat = seatOnFace(point, normal, offset);
    for (const double angle : {seat.turn.x, seat.turn.y, seat.turn.z}) {
        CHECK(std::abs(angle * 10.0 - std::round(angle * 10.0)) < 1e-9);
    }
    for (const double value : {seat.foot.x, seat.foot.y, seat.foot.z}) {
        CHECK(std::abs(value * 1000.0 - std::round(value * 1000.0)) < 1e-6);
    }
    // The underside still lies on the point (the rounding moves it by a few micrometres).
    const Point3d foot = placed(seat, offset, {0, 0, 0});
    CHECK(foot.x == near(point.x));
    CHECK(foot.y == near(point.y));
    CHECK(foot.z == near(point.z));
    // The shape stands out of the face within a tenth of a degree of the face's direction.
    const Point3d top = placed(seat, offset, {0, 0, 20});
    const double length = std::sqrt(0.31 * 0.31 + 0.62 * 0.62 + 0.72 * 0.72);
    CHECK((top.x - foot.x) / 20.0 == Approx(0.31 / length).margin(2e-3));
    CHECK((top.y - foot.y) / 20.0 == Approx(-0.62 / length).margin(2e-3));
    CHECK((top.z - foot.z) / 20.0 == Approx(0.72 / length).margin(2e-3));
}
