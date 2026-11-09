// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Where the curves of a sketch cross (AP-2.1e1): lines, circles, arcs and splines, touches
// included, with the parameter of each crossing on each curve.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

std::unique_ptr<GeometryKernel> kernel() {
    return occt::makeKernel();
}

SketchCurve line(Point2d a, Point2d b) {
    return {SketchCurveKind::line, {a, b}};
}
SketchCurve circle(Point2d middle, double radius) {
    return {SketchCurveKind::circle, {middle, {middle.x + radius, middle.y}}};
}
// counter-clockwise from the angle `from` to `to` (degrees) round `middle`
SketchCurve arc(Point2d middle, double radius, double from, double to) {
    const double a = from * kPi / 180.0;
    const double b = to * kPi / 180.0;
    return {SketchCurveKind::arc,
            {middle,
             {middle.x + radius * std::cos(a), middle.y + radius * std::sin(a)},
             {middle.x + radius * std::cos(b), middle.y + radius * std::sin(b)}}};
}

} // namespace

TEST_CASE("sketch intersections: two lines cross once, with the distance on each",
          "[sketch][intersections]") {
    const auto k = kernel();
    const auto r = k->sketchIntersections({line({0, 0}, {10, 0}), line({4, -3}, {4, 5})});
    REQUIRE(r.size() == 2);
    REQUIRE(r[0].size() == 1);
    REQUIRE(r[1].size() == 1);
    CHECK(r[0][0].parameter == Approx(4.0));
    CHECK(r[0][0].point.x == Approx(4.0));
    CHECK(r[0][0].point.y == Approx(0.0).margin(1e-12));
    CHECK(r[0][0].other == 1);
    CHECK(r[1][0].parameter == Approx(3.0));
    CHECK(r[1][0].other == 0);
    CHECK(r[1][0].otherParameter == Approx(4.0));
}

TEST_CASE("sketch intersections: lines that do not meet or lie on each other have none",
          "[sketch][intersections]") {
    const auto k = kernel();
    SECTION("apart") {
        const auto r = k->sketchIntersections({line({0, 0}, {10, 0}), line({12, -3}, {12, 5})});
        CHECK(r[0].empty());
        CHECK(r[1].empty());
    }
    SECTION("parallel") {
        const auto r = k->sketchIntersections({line({0, 0}, {10, 0}), line({0, 1}, {10, 1})});
        CHECK(r[0].empty());
    }
    SECTION("on top of each other") {
        const auto r = k->sketchIntersections({line({0, 0}, {10, 0}), line({5, 0}, {15, 0})});
        CHECK(r[0].empty());
    }
    SECTION("nothing at all") {
        CHECK(k->sketchIntersections({}).empty());
        CHECK(k->sketchIntersections({line({0, 0}, {1, 1})})[0].empty());
    }
}

TEST_CASE("sketch intersections: lines that meet at their ends touch", "[sketch][intersections]") {
    const auto k = kernel();
    const auto r = k->sketchIntersections({line({0, 0}, {10, 0}), line({10, 0}, {10, 7})});
    REQUIRE(r[0].size() == 1);
    CHECK(r[0][0].parameter == Approx(10.0));
    REQUIRE(r[1].size() == 1);
    CHECK(r[1][0].parameter == Approx(0.0).margin(1e-12));
}

TEST_CASE("sketch intersections: a line and a circle meet in two points, one (a touch) or none",
          "[sketch][intersections]") {
    const auto k = kernel();
    SECTION("two points") {
        const auto r = k->sketchIntersections({line({-10, 3}, {10, 3}), circle({0, 0}, 5)});
        REQUIRE(r[0].size() == 2);
        CHECK(r[0][0].parameter == Approx(6.0));
        CHECK(r[0][1].parameter == Approx(14.0));
        CHECK(r[0][0].point.x == Approx(-4.0));
        CHECK(r[0][1].point.x == Approx(4.0));
        REQUIRE(r[1].size() == 2);
        // seen from the circle: (4, 3) is at atan2(3, 4), (-4, 3) at π - that
        CHECK(r[1][0].parameter == Approx(std::atan2(3.0, 4.0)));
        CHECK(r[1][1].parameter == Approx(kPi - std::atan2(3.0, 4.0)));
    }
    SECTION("tangential") {
        const auto r = k->sketchIntersections({line({-10, 5}, {10, 5}), circle({0, 0}, 5)});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].parameter == Approx(10.0));
        CHECK(r[0][0].point.y == Approx(5.0));
        REQUIRE(r[1].size() == 1);
        CHECK(r[1][0].parameter == Approx(kPi / 2.0));
    }
    SECTION("past the circle") {
        const auto r = k->sketchIntersections({line({-10, 6}, {10, 6}), circle({0, 0}, 5)});
        CHECK(r[0].empty());
        CHECK(r[1].empty());
    }
    SECTION("a line that stops inside the circle meets it once") {
        const auto r = k->sketchIntersections({line({0, 0}, {10, 0}), circle({0, 0}, 5)});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].parameter == Approx(5.0));
    }
}

TEST_CASE("sketch intersections: circles meet in two points, one or none",
          "[sketch][intersections]") {
    const auto k = kernel();
    SECTION("two points") {
        const auto r = k->sketchIntersections({circle({0, 0}, 5), circle({6, 0}, 5)});
        REQUIRE(r[0].size() == 2);
        CHECK(r[0][0].point.x == Approx(3.0));
        CHECK(std::abs(r[0][0].point.y) == Approx(4.0));
        CHECK(r[0][1].point.x == Approx(3.0));
        REQUIRE(r[1].size() == 2);
    }
    SECTION("touching from outside") {
        const auto r = k->sketchIntersections({circle({0, 0}, 5), circle({8, 0}, 3)});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].point.x == Approx(5.0));
        CHECK(r[0][0].parameter == Approx(0.0).margin(1e-9));
    }
    SECTION("touching from inside") {
        const auto r = k->sketchIntersections({circle({0, 0}, 5), circle({2, 0}, 3)});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].point.x == Approx(5.0));
    }
    SECTION("inside each other, apart, or the same") {
        CHECK(k->sketchIntersections({circle({0, 0}, 5), circle({1, 0}, 1)})[0].empty());
        CHECK(k->sketchIntersections({circle({0, 0}, 5), circle({20, 0}, 1)})[0].empty());
        CHECK(k->sketchIntersections({circle({0, 0}, 5), circle({0, 0}, 5)})[0].empty());
    }
}

TEST_CASE("sketch intersections: an arc only meets what lies on the arc",
          "[sketch][intersections]") {
    const auto k = kernel();
    // the upper half of a circle of radius 5
    const SketchCurve half = arc({0, 0}, 5, 0, 180);
    SECTION("a line through the circle meets the arc once") {
        const auto r = k->sketchIntersections({half, line({-10, 3}, {10, 3})});
        REQUIRE(r[0].size() == 2); // y = 3 cuts the upper half twice
        const auto below = k->sketchIntersections({half, line({-10, -3}, {10, -3})});
        CHECK(below[0].empty()); // the lower half is not drawn
    }
    SECTION("parameters run counter-clockwise from the start") {
        const auto r = k->sketchIntersections({half, line({0, -10}, {0, 10})});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].parameter == Approx(kPi / 2.0));
        CHECK(r[0][0].point.y == Approx(5.0));
        CHECK(r[1][0].parameter == Approx(15.0));
    }
    SECTION("the ends of the arc count") {
        const auto r = k->sketchIntersections({half, line({5, -2}, {5, 2})});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].parameter == Approx(0.0).margin(1e-9));
        const auto end = k->sketchIntersections({half, line({-5, -2}, {-5, 2})});
        REQUIRE(end[0].size() == 1);
        CHECK(end[0][0].parameter == Approx(kPi));
    }
    SECTION("an arc and a circle") {
        const auto r = k->sketchIntersections({half, circle({6, 0}, 5)});
        REQUIRE(r[0].size() == 1); // (3, 4) is on the upper half, (3, -4) is not
        CHECK(r[0][0].point.y == Approx(4.0));
    }
}

TEST_CASE("sketch intersections: a curve crosses lines and circles", "[sketch][intersections]") {
    const auto k = kernel();
    const SketchCurve wave{SketchCurveKind::spline, {{0, 0}, {10, 8}, {20, -4}, {35, 6}, {50, 0}}};
    SECTION("a line across the curve") {
        const auto r = k->sketchIntersections({wave, line({15, -20}, {15, 20})});
        REQUIRE(r[0].size() == 1);
        CHECK(r[0][0].point.x == Approx(15.0).margin(1e-6));
        REQUIRE(r[1].size() == 1);
        // the point lies on the line: its distance from the start of the line
        CHECK(r[1][0].parameter == Approx(r[0][0].point.y + 20.0).margin(1e-6));
        // between the points (10, 8) and (20, -4), so the parameter is between theirs
        CHECK(r[0][0].parameter > 12.8);
        CHECK(r[0][0].parameter < 28.0);
    }
    SECTION("a line that stops short of the curve") {
        const auto r = k->sketchIntersections({wave, line({15, -20}, {15, -10})});
        CHECK(r[0].empty());
    }
    SECTION("a line through the points of the curve meets it at them") {
        const auto r = k->sketchIntersections({wave, line({10, -20}, {10, 20})});
        bool atPoint = false;
        for (const SketchCrossing& c : r[0]) {
            atPoint =
                atPoint || (std::abs(c.point.x - 10.0) < 1e-6 && std::abs(c.point.y - 8.0) < 1e-6);
        }
        CHECK(atPoint);
    }
    SECTION("a circle") {
        const auto r = k->sketchIntersections({wave, circle({20, 0}, 6)});
        CHECK_FALSE(r[0].empty());
        for (const SketchCrossing& c : r[0]) {
            CHECK(std::hypot(c.point.x - 20.0, c.point.y) == Approx(6.0).margin(1e-6));
        }
    }
    SECTION("two curves") {
        const SketchCurve other{SketchCurveKind::spline, {{0, 4}, {25, -2}, {50, 5}}};
        const auto r = k->sketchIntersections({wave, other});
        CHECK_FALSE(r[0].empty());
        CHECK(r[0].size() == r[1].size());
    }
}

TEST_CASE("sketch intersections: crossings at one point are one", "[sketch][intersections]") {
    const auto k = kernel();
    // three lines through (5, 0): the first has one crossing there, not two
    const auto r = k->sketchIntersections(
        {line({0, 0}, {10, 0}), line({5, -5}, {5, 5}), line({0, -5}, {10, 5})});
    REQUIRE(r[0].size() == 1);
    CHECK(r[0][0].parameter == Approx(5.0));
}

TEST_CASE("sketch intersections: a curve that cannot be used is refused",
          "[sketch][intersections]") {
    const auto k = kernel();
    CHECK_THROWS_AS(k->sketchIntersections({line({1, 1}, {1, 1})}), GeometryError);
    CHECK_THROWS_AS(k->sketchIntersections({{SketchCurveKind::line, {{0, 0}}}}), GeometryError);
    CHECK_THROWS_AS(k->sketchIntersections({circle({0, 0}, 0.0)}), GeometryError);
    CHECK_THROWS_AS(k->sketchIntersections({line({0, 0}, {std::nan(""), 1})}), GeometryError);
    CHECK_THROWS_AS(k->sketchIntersections({{SketchCurveKind::spline, {{1, 1}, {1, 1}}}}),
                    GeometryError);
}

TEST_CASE("sketch intersections: the carrier of a line or arc goes on past its ends",
          "[sketch][intersections]") {
    const auto k = kernel();
    SECTION("a line meets a wall it does not reach") {
        const auto r = k->sketchCarrierCrossings(
            line({0, 0}, {10, 0}), {line({25, -5}, {25, 5}), line({-8, -5}, {-8, 5})});
        REQUIRE(r.size() == 2);
        CHECK(r[0].parameter == Approx(-8.0));
        CHECK(r[0].other == 1);
        CHECK(r[1].parameter == Approx(25.0));
        CHECK(r[1].other == 0);
    }
    SECTION("the others count as drawn: a wall that stops short is not met") {
        const auto r = k->sketchCarrierCrossings(line({0, 0}, {10, 0}), {line({25, 2}, {25, 5})});
        CHECK(r.empty());
    }
    SECTION("an arc goes on round its circle") {
        const auto r = k->sketchCarrierCrossings(arc({0, 0}, 5, 0, 90), {line({-10, 3}, {10, 3})});
        REQUIRE(r.size() == 2);
        // the angles from the start of the arc, counter-clockwise: (4, 3) is before the end of
        // the arc, (-4, 3) is beyond it
        CHECK(r[0].parameter == Approx(std::atan2(3.0, 4.0)));
        CHECK(r[1].parameter == Approx(kPi - std::atan2(3.0, 4.0)));
    }
    SECTION("a spline in the way is met") {
        const SketchCurve wave{SketchCurveKind::spline, {{30, -10}, {32, 0}, {30, 10}}};
        const auto r = k->sketchCarrierCrossings(line({0, 0}, {10, 0}), {wave});
        REQUIRE(r.size() == 1);
        CHECK(r[0].parameter == Approx(32.0).margin(1e-6));
    }
    SECTION("a spline has no carrier") {
        CHECK_THROWS_AS(
            k->sketchCarrierCrossings({SketchCurveKind::spline, {{0, 0}, {1, 1}, {2, 0}}}, {}),
            GeometryError);
    }
}
