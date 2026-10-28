// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The geometry of dragging (AP-1.6): the pointer is a ray, laid on a plane through what was
// grabbed.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QVector3D>

#include <cmath>

#include "fission/interaction/drag.h"

using Catch::Approx;
using fission::interaction::AxisDrag;
using fission::interaction::PlaneDrag;
using fission::interaction::Ray;
using fission::interaction::RingDrag;
using fission::interaction::snapped;
using fission::interaction::snapStep;

namespace {

// A screen that is not used: the axis is seen from the side.
const AxisDrag::Screen kUnused{QPointF(), QPointF(1.0, 0.0), 1.0};

// A ray from an eye above and in front, through a point.
Ray through(const QVector3D& point, const QVector3D& eye = {0.0F, -100.0F, 100.0F}) {
    return {eye, (point - eye).normalized()};
}

} // namespace

TEST_CASE("drag: along an axis the pointer is followed one to one", "[interaction][drag]") {
    const auto drag = AxisDrag::grab(through({5, 0, 0}), {0, 0, 0}, {1, 0, 0}, kUnused);
    REQUIRE(drag);
    CHECK_FALSE(drag->onScreen());
    CHECK(*drag->moved(through({5, 0, 0}), {}) == Approx(0.0).margin(1e-4));
    CHECK(*drag->moved(through({17, 0, 0}), {}) == Approx(12.0).margin(1e-4));
    CHECK(*drag->moved(through({-3, 0, 0}), {}) == Approx(-8.0).margin(1e-4));
    // A pointer beside the axis counts by how far along it is.
    CHECK(*drag->moved(through({17, 4, 2}), {}) == Approx(12.0).margin(0.5));
}

TEST_CASE("drag: an axis pointing at the viewer is dragged on the screen", "[interaction][drag]") {
    const QVector3D eye(0, 0, 100);
    const AxisDrag::Screen screen{QPointF(100.0, 100.0), QPointF(0.0, -1.0), 2.0};
    const auto drag = AxisDrag::grab(through({0, 0, 0}, eye), {0, 0, 0}, {0, 0, 1}, screen);
    REQUIRE(drag);
    CHECK(drag->onScreen());
    // 20 pixels up the screen at 2 pixels per millimetre: 10 mm, wherever the ray goes.
    CHECK(*drag->moved(through({0, 0, 0}, eye), QPointF(103.0, 80.0)) == Approx(10.0));
    CHECK(*drag->moved(through({0, 0, 0}, eye), QPointF(100.0, 110.0)) == Approx(-5.0));
    CHECK_FALSE(AxisDrag::grab(through({0, 0, 0}, eye), {0, 0, 0}, {1, 0, 0}, screen)->onScreen());
}

TEST_CASE("drag: how the screen shows an axis", "[interaction][drag]") {
    // A plain projection looking straight down from 100 mm above: 3 pixels per millimetre.
    const auto project = [](const QVector3D& p) { return QPointF(3.0 * p.x(), -3.0 * p.y()); };
    const QVector3D eye(0, 0, 100);
    const auto across = AxisDrag::screenOf(QPointF(), {0, 0, 0}, {1, 0, 0}, eye, project);
    CHECK(across.way.x() == Approx(1.0));
    CHECK(across.pixelsPerMillimetre == Approx(3.0));
    // Straight at the viewer, from above: up rises, at three quarters of the rate across.
    const auto up = AxisDrag::screenOf(QPointF(), {0, 0, 0}, {0, 0, 1}, eye, project);
    CHECK(up.way.y() == Approx(-1.0));
    CHECK(up.pixelsPerMillimetre == Approx(0.75 * 3.0));
    const auto down = AxisDrag::screenOf(QPointF(), {0, 0, 0}, {0, 0, -1}, eye, project);
    CHECK(down.way.y() == Approx(1.0));
    // A level axis pointing at a viewer in front: nearer is down the screen, as the bed shows it
    // from a little above.
    const QVector3D front(0, -100, 0);
    const auto flat = [](const QVector3D& p) { return QPointF(3.0 * p.x(), -3.0 * p.z()); };
    const auto near = AxisDrag::screenOf(QPointF(), {0, 0, 0}, {0, -1, 0}, front, flat);
    CHECK(near.way.y() == Approx(1.0));
}

TEST_CASE("drag: near the vanishing point of an axis the pointer is not followed",
          "[interaction][drag]") {
    // The axis runs away from the viewer; a ray that meets the drag plane very far away would
    // throw the value out of sight.
    const QVector3D eye(0, -100, 70);
    const auto drag = AxisDrag::grab(through({0, 0, 0}, eye), {0, 0, 0}, {0, 1, 0}, kUnused);
    REQUIRE(drag);
    REQUIRE_FALSE(drag->onScreen());
    CHECK(drag->moved(through({0, 50, 0}, eye), {}));
    CHECK_FALSE(drag->moved(through({0, 5000, 0}, eye), {}));
}

TEST_CASE("drag: in a plane the pointer moves within it", "[interaction][drag]") {
    const auto drag = PlaneDrag::grab(through({2, 3, 0}), {0, 0, 0}, {0, 0, 1});
    REQUIRE(drag);
    const auto moved = drag->moved(through({12, -4, 0}));
    REQUIRE(moved);
    CHECK(moved->x() == Approx(10.0).margin(1e-3));
    CHECK(moved->y() == Approx(-7.0).margin(1e-3));
    CHECK(moved->z() == Approx(0.0).margin(1e-6));
    // Seen edge on, a plane is not grabbed.
    CHECK_FALSE(PlaneDrag::grab(through({0, 0, 0}, {-100, 0, 0}), {0, 0, 0}, {0, 0, 1}));
    // A ray that does not meet the plane in front of the eye moves nothing, nor one that meets
    // it near the horizon.
    CHECK_FALSE(drag->moved({{0, -100, 100}, {0, 0, 1}}));
    CHECK_FALSE(drag->moved(through({0, 5000, 0})));
}

TEST_CASE("drag: positions snap to the step of the zoom", "[interaction][drag]") {
    CHECK(snapStep(100.0) == Approx(0.1));
    CHECK(snapStep(5.0) == Approx(1.0));
    CHECK(snapStep(0.2) == Approx(10.0));
    CHECK(snapped(12.4, 1.0) == Approx(12.0));
    CHECK(snapped(12.26, 0.1) == Approx(12.3));
    CHECK(snapped(-0.2, 1.0) == 0.0);
    CHECK_FALSE(std::signbit(snapped(-0.2, 1.0))); // no "-0" in a field
}

TEST_CASE("ring: going round the axis turns by the same angle", "[interaction][drag][ring]") {
    // Looking down onto a ring round Z, 3 pixels per millimetre.
    const QVector3D eye(0, 0, 200);
    const auto project = [](const QVector3D& p) { return QPointF(3.0 * p.x(), -3.0 * p.y()); };
    auto drag =
        RingDrag::grab(through({10, 0, 0}, eye), QPointF(30, 0), {0, 0, 0}, {0, 0, 1}, project);
    REQUIRE(drag);
    CHECK_FALSE(drag->onScreen());
    CHECK(*drag->moved(through({10, 0, 0}, eye), {}) == Approx(0.0).margin(1e-4));
    // A quarter of the way round counter-clockwise (seen from above) is +90 degrees.
    CHECK(*drag->moved(through({0, 10, 0}, eye), {}) == Approx(90.0).margin(1e-3));
    CHECK(*drag->moved(through({-10, 0, 0}, eye), {}) == Approx(180.0).margin(1e-3));
    // On round the same way: it goes on counting past 180 …
    CHECK(*drag->moved(through({0, -10, 0}, eye), {}) == Approx(270.0).margin(1e-3));
    CHECK(*drag->moved(through({10, 0, 0}, eye), {}) == Approx(360.0).margin(1e-3));
    // … and back again.
    CHECK(*drag->moved(through({0, -10, 0}, eye), {}) == Approx(270.0).margin(1e-3));
    // How far from the middle the pointer is does not matter.
    CHECK(*drag->moved(through({-40, 0, 0}, eye), {}) == Approx(180.0).margin(1e-3));
}

TEST_CASE("ring: each ring turns the right way round its own axis", "[interaction][drag][ring]") {
    const QVector3D eye(100, 100, 100);
    const auto project = [](const QVector3D& p) { return QPointF(p.x(), p.y()); };
    // A point on the ring, then the point a quarter of a turn on (right-hand rule).
    const struct {
        QVector3D axis;
        QVector3D from;
        QVector3D to;
    } rings[3] = {{{1, 0, 0}, {0, 10, 0}, {0, 0, 10}},
                  {{0, 1, 0}, {0, 0, 10}, {10, 0, 0}},
                  {{0, 0, 1}, {10, 0, 0}, {0, 10, 0}}};
    for (const auto& ring : rings) {
        auto drag = RingDrag::grab(through(ring.from, eye), {}, {0, 0, 0}, ring.axis, project);
        REQUIRE(drag);
        CHECK(*drag->moved(through(ring.to, eye), {}) == Approx(90.0).margin(1e-2));
    }
}

TEST_CASE("ring: seen edge on it is dragged along the ring on the screen",
          "[interaction][drag][ring]") {
    // A ring round Z seen from 10 degrees above its plane: a thin ellipse. The camera looks along
    // (0, cos, -sin); the screen's "up" is (0, sin, cos).
    constexpr double kAbove = 10.0 * 3.14159265358979323846 / 180.0;
    const float c = static_cast<float>(std::cos(kAbove));
    const float s = static_cast<float>(std::sin(kAbove));
    const QVector3D eye(0, -200 * c, 200 * s);
    const QVector3D up(0, s, c);
    const auto project = [up](const QVector3D& p) {
        return QPointF(2.0 * p.x(), -2.0 * QVector3D::dotProduct(p, up));
    };
    // Grabbed on the near side of the ring, where it runs across the screen.
    const QPointF start = project({0, -20, 0});
    auto drag = RingDrag::grab(through({0, -20, 0}, eye), start, {0, 0, 0}, {0, 0, 1}, project);
    REQUIRE(drag);
    CHECK(drag->onScreen());
    const Ray ray = through({0, -20, 0}, eye);
    CHECK(*drag->moved(ray, start) == Approx(0.0).margin(1e-6));
    // Counter-clockwise seen from above runs to the right there. The ring is 20 mm wide at
    // 2 pixels per millimetre: 40 pixels to the radian.
    CHECK(*drag->moved(ray, start + QPointF(40, 0)) == Approx(57.29578).margin(0.5));
    CHECK(*drag->moved(ray, start + QPointF(-20, 0)) == Approx(-28.64789).margin(0.5));
    // Up and down the screen turns nothing there.
    CHECK(*drag->moved(ray, start + QPointF(0, 30)) == Approx(0.0).margin(1.0));
}

TEST_CASE("ring: seen exactly edge on it still turns, wherever on the line it is taken",
          "[interaction][drag][ring]") {
    // The ring round X seen straight from above: the eye is in the plane of the ring, which is a
    // line up the screen. Screen "up" is +Y, 2 pixels to the millimetre; the ring is 20 mm wide.
    const QVector3D eye(0, 0, 200);
    const auto project = [](const QVector3D& p) { return QPointF(2.0 * p.x(), -2.0 * p.y()); };
    for (const float taken : {20.0F, 15.0F, -8.0F}) {
        CAPTURE(taken);
        const QVector3D point(0, taken, 0);
        const QPointF start = project(point);
        auto drag = RingDrag::grab(through(point, eye), start, {0, 0, 0}, {1, 0, 0}, project, 20.0);
        REQUIRE(drag);
        CHECK(drag->onScreen());
        const Ray ray = through(point, eye);
        // The top of the ring moves towards -Y (down the screen) when it turns round +X. One ring
        // radius along the line (40 pixels) is one radian, also at the end of the line, where
        // the ring runs into the screen.
        CHECK(*drag->moved(ray, start + QPointF(0, 40)) == Approx(57.29578).margin(0.5));
        CHECK(*drag->moved(ray, start + QPointF(0, -20)) == Approx(-28.64789).margin(0.5));
        // Across the line turns nothing.
        CHECK(*drag->moved(ray, start + QPointF(30, 0)) == Approx(0.0).margin(1.0));
        // The wedge starts where the ring was taken: on the half towards the eye, under the
        // pointer.
        const QVector3D grabbed = drag->ringPoint(drag->grabbedAt(), 20.0);
        CHECK(grabbed.z() >= 0.0F);
        CHECK(grabbed.y() == Approx(taken).margin(1.5));
    }
}

TEST_CASE("ring: the turning step is 15 degrees", "[interaction][drag][ring]") {
    CHECK(snapped(7.0, fission::interaction::kTurnStepDegrees) == Approx(0.0));
    CHECK(snapped(8.0, fission::interaction::kTurnStepDegrees) == Approx(15.0));
    CHECK(snapped(-100.0, fission::interaction::kTurnStepDegrees) == Approx(-105.0));
}
