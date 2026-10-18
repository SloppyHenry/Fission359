// SPDX-FileCopyrightText: Fission3D contributors
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
using fission::interaction::snapped;
using fission::interaction::snapStep;

namespace {

// A ray from an eye above and in front, through a point.
Ray through(const QVector3D& point, const QVector3D& eye = {0.0F, -100.0F, 100.0F}) {
    return {eye, (point - eye).normalized()};
}

} // namespace

TEST_CASE("drag: along an axis the pointer is followed one to one", "[interaction][drag]") {
    const auto drag = AxisDrag::grab(through({5, 0, 0}), {0, 0, 0}, {1, 0, 0});
    REQUIRE(drag);
    CHECK(*drag->moved(through({5, 0, 0})) == Approx(0.0).margin(1e-4));
    CHECK(*drag->moved(through({17, 0, 0})) == Approx(12.0).margin(1e-4));
    CHECK(*drag->moved(through({-3, 0, 0})) == Approx(-8.0).margin(1e-4));
    // A pointer beside the axis counts by how far along it is.
    CHECK(*drag->moved(through({17, 4, 2})) == Approx(12.0).margin(0.5));
}

TEST_CASE("drag: an axis pointing at the viewer cannot be grabbed", "[interaction][drag]") {
    const QVector3D eye(0, 0, 100);
    CHECK_FALSE(AxisDrag::grab(through({0, 0, 0}, eye), {0, 0, 0}, {0, 0, 1}));
    CHECK(AxisDrag::grab(through({0, 0, 0}, eye), {0, 0, 0}, {1, 0, 0}));
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
    // A ray that does not meet the plane in front of the eye moves nothing.
    CHECK_FALSE(drag->moved({{0, -100, 100}, {0, 0, 1}}));
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
