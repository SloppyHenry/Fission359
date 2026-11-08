// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The geometry of the drawing tools of the sketch mode (AP-2.1c2): arcs, the arc that joins a
// line tangentially, the slot and the regular polygon, as pure functions.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "fission/interaction/sketch_shapes.h"

using Catch::Approx;
using namespace fission::interaction;
using fission::core::SketchElement;
using fission::core::SketchElementKind;
using fission::core::SketchPoint;

namespace {

constexpr double kPi = std::numbers::pi;

void checkPoint(const SketchPoint& point, double u, double v) {
    CHECK(point.u == Approx(u).margin(1e-9));
    CHECK(point.v == Approx(v).margin(1e-9));
}

double distance(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(b.u - a.u, b.v - a.v);
}

// The arc as {middle, start, end}: start and end lie on the circle round the middle.
void checkArcIsRound(const SketchElement& arc) {
    REQUIRE(arc.kind == SketchElementKind::arc);
    REQUIRE(arc.points.size() == 3);
    CHECK(distance(arc.points[0], arc.points[1]) ==
          Approx(distance(arc.points[0], arc.points[2])).margin(1e-9));
}

} // namespace

TEST_CASE("sketch shapes: an arc through three points is counter-clockwise, clockwise swaps",
          "[sketchshapes]") {
    const auto up = arcThroughThreePoints({10, 0}, {0, 10}, {-10, 0});
    REQUIRE(up);
    checkArcIsRound(*up);
    checkPoint(up->points[0], 0, 0);
    checkPoint(up->points[1], 10, 0); // walked a -> c over the top = counter-clockwise
    checkPoint(up->points[2], -10, 0);

    const auto clockwise = arcThroughThreePoints({-10, 0}, {0, 10}, {10, 0});
    REQUIRE(clockwise);
    checkPoint(clockwise->points[0], 0, 0);
    checkPoint(clockwise->points[1], 10, 0); // start and end are swapped
    checkPoint(clockwise->points[2], -10, 0);

    CHECK_FALSE(arcThroughThreePoints({0, 0}, {5, 5}, {10, 10}));
    CHECK_FALSE(arcThroughThreePoints({0, 0}, {0, 0}, {10, 0}));
}

TEST_CASE("sketch shapes: an arc over a chord with a radius", "[sketchshapes]") {
    const auto left = arcOverChord({-5, 0}, {5, 0}, 1, 13.0);
    REQUIRE(left);
    checkArcIsRound(*left);
    checkPoint(left->points[0], 0, -12); // bulging up: the middle lies below
    checkPoint(left->points[1], 5, 0);   // clockwise: swapped
    checkPoint(left->points[2], -5, 0);

    const auto right = arcOverChord({-5, 0}, {5, 0}, -1, 13.0);
    REQUIRE(right);
    checkPoint(right->points[0], 0, 12);
    checkPoint(right->points[1], -5, 0);
    checkPoint(right->points[2], 5, 0);

    const auto half = arcOverChord({-5, 0}, {5, 0}, 1, 5.0); // exactly half the chord
    REQUIRE(half);
    checkPoint(half->points[0], 0, 0);

    CHECK_FALSE(arcOverChord({-5, 0}, {5, 0}, 1, 4.99)); // smaller than half the chord
    CHECK_FALSE(arcOverChord({1, 1}, {1, 1}, 1, 4.0));
}

TEST_CASE("sketch shapes: an arc by a turn round its middle", "[sketchshapes]") {
    const auto ccw = arcBySweep({0, 0}, {10, 0}, kPi / 2.0);
    REQUIRE(ccw);
    checkArcIsRound(*ccw);
    checkPoint(ccw->points[1], 10, 0);
    checkPoint(ccw->points[2], 0, 10);

    const auto cw = arcBySweep({0, 0}, {10, 0}, -kPi / 2.0);
    REQUIRE(cw);
    checkPoint(cw->points[1], 0, -10); // swapped
    checkPoint(cw->points[2], 10, 0);

    const auto big = arcBySweep({0, 0}, {10, 0}, 1.5 * kPi);
    REQUIRE(big);
    checkPoint(big->points[2], 0, -10);

    CHECK_FALSE(arcBySweep({0, 0}, {10, 0}, 0.0));
    CHECK_FALSE(arcBySweep({0, 0}, {10, 0}, 2.0 * kPi));
    CHECK_FALSE(arcBySweep({0, 0}, {0.001, 0}, 1.0));
}

TEST_CASE("sketch shapes: the arc that joins a line has the same direction as the line",
          "[sketchshapes]") {
    // left of the line: counter-clockwise
    const auto left = tangentArc({0, 0}, {1, 0}, {10, 10});
    REQUIRE(left);
    CHECK_FALSE(left->clockwise);
    CHECK(left->radius == Approx(10.0));
    checkArcIsRound(left->arc);
    checkPoint(left->arc.points[0], 0, 10);
    checkPoint(left->arc.points[1], 0, 0);
    checkPoint(left->arc.points[2], 10, 10);
    checkPoint(left->end, 10, 10);
    checkPoint(left->endDirection, 0, 1); // counter-clockwise at the right of the circle: up

    // right of the line: clockwise, the element has its end first
    const auto right = tangentArc({0, 0}, {1, 0}, {10, -10});
    REQUIRE(right);
    CHECK(right->clockwise);
    checkPoint(right->arc.points[0], 0, -10);
    checkPoint(right->arc.points[1], 10, -10);
    checkPoint(right->arc.points[2], 0, 0);
    checkPoint(right->endDirection, 0, -1);

    // the walk starts along the direction given: radial -> tangent, on 1e-9
    for (const SketchPoint& direction :
         {SketchPoint{0.6, 0.8}, SketchPoint{-1, 0}, SketchPoint{0, -1}}) {
        for (const SketchPoint& to : {SketchPoint{7, 3}, SketchPoint{-4, 9}, SketchPoint{2, -6}}) {
            const SketchPoint from{1.5, -2.5};
            const auto arc = tangentArc(from, direction, to);
            if (!arc) {
                continue;
            }
            const SketchPoint& middle = arc->arc.points[0];
            const double r = distance(middle, from);
            CHECK(r == Approx(arc->radius).margin(1e-9));
            const double ru = (from.u - middle.u) / r;
            const double rv = (from.v - middle.v) / r;
            const SketchPoint tangent =
                arc->clockwise ? SketchPoint{rv, -ru} : SketchPoint{-rv, ru};
            checkPoint(tangent, direction.u, direction.v);
            CHECK(distance(middle, arc->end) == Approx(r).margin(1e-9));
        }
    }
}

TEST_CASE("sketch shapes: the arc that joins a line with a typed radius", "[sketchshapes]") {
    const auto arc = tangentArc({0, 0}, {1, 0}, {10, 10}, 5.0);
    REQUIRE(arc);
    CHECK(arc->radius == Approx(5.0));
    checkPoint(arc->arc.points[0], 0, 5);
    CHECK(distance(arc->arc.points[0], arc->end) == Approx(5.0).margin(1e-9));
    // the end lies towards the pointer, seen from the middle
    CHECK(arc->end.u > 0.0);
    CHECK(arc->end.v > 5.0);
}

TEST_CASE("sketch shapes: no arc joins a line towards a point on that line", "[sketchshapes]") {
    CHECK_FALSE(tangentArc({0, 0}, {1, 0}, {10, 0}));
    CHECK_FALSE(tangentArc({0, 0}, {1, 0}, {-10, 0}));
    CHECK_FALSE(tangentArc({0, 0}, {1, 0}, {0, 0}));
    CHECK_FALSE(tangentArc({0, 0}, {1, 0}, {10, 10}, 0.0));
}

TEST_CASE("sketch shapes: a slot is two lines and two half circles with closed ends",
          "[sketchshapes]") {
    const SketchBatch slot = slotElements({0, 0}, {10, 0}, 4.0);
    REQUIRE(slot.size() == 4);
    CHECK(slot[0].kind == SketchElementKind::line);
    CHECK(slot[1].kind == SketchElementKind::arc);
    CHECK(slot[2].kind == SketchElementKind::line);
    CHECK(slot[3].kind == SketchElementKind::arc);
    checkPoint(slot[0].points[0], 0, -2);
    checkPoint(slot[0].points[1], 10, -2);
    checkPoint(slot[1].points[0], 10, 0);
    checkPoint(slot[1].points[1], 10, -2);
    checkPoint(slot[1].points[2], 10, 2);
    checkPoint(slot[3].points[0], 0, 0);
    // each end is exactly the start of the next element, all round
    CHECK(slot[0].points[1] == slot[1].points[1]);
    CHECK(slot[1].points[2] == slot[2].points[0]);
    CHECK(slot[2].points[1] == slot[3].points[1]);
    CHECK(slot[3].points[2] == slot[0].points[0]);
    checkArcIsRound(slot[1]);
    checkArcIsRound(slot[3]);
    CHECK(distance(slot[1].points[0], slot[1].points[1]) == Approx(2.0));

    // turned: the same slot
    const SketchBatch turned = slotElements({1, 1}, {1, 11}, 4.0);
    REQUIRE(turned.size() == 4);
    CHECK(turned[0].points[1] == turned[1].points[1]);
    CHECK(turned[3].points[2] == turned[0].points[0]);
    CHECK(distance(turned[3].points[0], turned[3].points[1]) == Approx(2.0).margin(1e-9));

    CHECK(slotElements({0, 0}, {10, 0}, 0.005).empty());
    CHECK(slotElements({0, 0}, {0.005, 0}, 4.0).empty());
}

TEST_CASE("sketch shapes: a regular polygon with 3, 6 and 64 sides is closed and even",
          "[sketchshapes]") {
    for (const int sides : {3, 6, 64}) {
        INFO("sides " << sides);
        const SketchBatch polygon = polygonLines({2, 3}, 10.0, sides, 0.3);
        REQUIRE(polygon.size() == static_cast<std::size_t>(sides));
        const double side = 2.0 * 10.0 * std::sin(kPi / sides);
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            CHECK(polygon[i].kind == SketchElementKind::line);
            CHECK(distance(polygon[i].points[0], polygon[i].points[1]) == Approx(side));
            CHECK(distance({2, 3}, polygon[i].points[0]) == Approx(10.0));
            // the end of one line is exactly the start of the next, the last closes the first
            CHECK(polygon[i].points[1] == polygon[(i + 1) % polygon.size()].points[0]);
        }
        checkPoint(polygon[0].points[0], 2 + 10.0 * std::cos(0.3), 3 + 10.0 * std::sin(0.3));
    }
    CHECK(polygonLines({0, 0}, 10.0, 2, 0.0).empty());
    CHECK(polygonLines({0, 0}, 0.001, 6, 0.0).empty());
}
