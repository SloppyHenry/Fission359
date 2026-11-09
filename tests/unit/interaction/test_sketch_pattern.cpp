// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Mirroring and patterns in the sketch (AP-2.1e3) as pure logic.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "fission/interaction/sketch_pattern.h"

using Catch::Approx;
using namespace fission::interaction;
using fission::core::SketchElement;
using fission::core::SketchElementKind;
using fission::core::SketchPoint;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kExact = 1e-9;

SketchElement line(std::string id, SketchPoint a, SketchPoint b, bool helper = false) {
    return {std::move(id), SketchElementKind::line, {a, b}, helper};
}
SketchElement circle(std::string id, SketchPoint middle, double radius) {
    return {
        std::move(id), SketchElementKind::circle, {middle, {middle.u + radius, middle.v}}, false};
}
SketchElement arc(std::string id, SketchPoint middle, double radius, double from, double to) {
    const double a = from * kPi / 180.0;
    const double b = to * kPi / 180.0;
    return {std::move(id),
            SketchElementKind::arc,
            {middle,
             {middle.u + radius * std::cos(a), middle.v + radius * std::sin(a)},
             {middle.u + radius * std::cos(b), middle.v + radius * std::sin(b)}},
            false};
}
SketchElement point(std::string id, SketchPoint at) {
    return {std::move(id), SketchElementKind::point, {at}, false};
}
SketchElement spline(std::string id, std::vector<SketchPoint> through) {
    return {std::move(id), SketchElementKind::spline, std::move(through), false};
}

bool is(const SketchPoint& p, double u, double v) {
    return std::abs(p.u - u) < kExact && std::abs(p.v - v) < kExact;
}

// the vertical axis u = 0
SketchElement axis() {
    return line("L9", {0, -50}, {0, 50});
}

} // namespace

TEST_CASE("Mirror: a line is mirrored exactly at the axis", "[sketch][pattern]") {
    const auto result = mirrorElements({axis(), line("L1", {10, 0}, {30, 5})}, {"L1"}, "L9");
    REQUIRE(result.notice == SketchCopyNotice::none);
    REQUIRE(result.edit.added.size() == 1);
    CHECK(result.edit.moves.empty());
    CHECK(result.edit.removed.empty());
    const SketchElement& copy = result.edit.added[0];
    CHECK(copy.id.empty());
    CHECK(copy.kind == SketchElementKind::line);
    CHECK(is(copy.points[0], -10, 0));
    CHECK(is(copy.points[1], -30, 5));
}

TEST_CASE("Mirror: a slanted axis", "[sketch][pattern]") {
    // the axis u = v: (3, 1) goes to (1, 3)
    const auto result =
        mirrorElements({line("L9", {0, 0}, {5, 5}), point("P1", {3, 1})}, {"P1"}, "L9");
    REQUIRE(result.edit.added.size() == 1);
    CHECK(is(result.edit.added[0].points[0], 1, 3));
}

TEST_CASE("Mirror: an arc swaps start and end and stays counter-clockwise", "[sketch][pattern]") {
    // from 10 to 100 degrees about (20, 0), radius 8
    const SketchElement source = arc("A1", {20, 0}, 8, 10, 100);
    const auto result = mirrorElements({axis(), source}, {"A1"}, "L9");
    REQUIRE(result.edit.added.size() == 1);
    const SketchElement& copy = result.edit.added[0];
    REQUIRE(copy.kind == SketchElementKind::arc);
    CHECK(is(copy.points[0], -20, 0));
    // the new start is the mirrored old end, the new end the mirrored old start
    CHECK(is(copy.points[1], -20 - 8 * std::cos(100 * kPi / 180), 8 * std::sin(100 * kPi / 180)));
    CHECK(is(copy.points[2], -20 - 8 * std::cos(10 * kPi / 180), 8 * std::sin(10 * kPi / 180)));
    // counter-clockwise from 80 to 170 degrees about its middle
    const double start =
        std::atan2(copy.points[1].v - copy.points[0].v, copy.points[1].u - copy.points[0].u);
    const double end =
        std::atan2(copy.points[2].v - copy.points[0].v, copy.points[2].u - copy.points[0].u);
    CHECK(start * 180 / kPi == Approx(80).margin(1e-9));
    CHECK(end * 180 / kPi == Approx(170).margin(1e-9));
}

TEST_CASE("Mirror: an arc whose end point is off the circle keeps its direction",
          "[sketch][pattern]") {
    SketchElement source = arc("A1", {20, 0}, 8, 0, 90);
    source.points[2] = {20, 30}; // only the direction (straight up) counts
    const auto result = mirrorElements({axis(), source}, {"A1"}, "L9");
    REQUIRE(result.edit.added.size() == 1);
    const SketchElement& copy = result.edit.added[0];
    CHECK(is(copy.points[1], -20, 8)); // the mirrored end on the circle
    CHECK(is(copy.points[2], -28, 0)); // the mirrored start
}

TEST_CASE("Mirror: circle, point and curve", "[sketch][pattern]") {
    const auto result = mirrorElements({axis(), circle("C1", {25, 5}, 4), point("P1", {7, -3}),
                                        spline("S1", {{5, 0}, {10, 10}, {20, 5}})},
                                       {"C1", "P1", "S1"}, "L9");
    REQUIRE(result.edit.added.size() == 3);
    // in the order of the sketch: circle, point, curve
    CHECK(result.edit.added[0].kind == SketchElementKind::circle);
    CHECK(is(result.edit.added[0].points[0], -25, 5));
    CHECK(std::hypot(result.edit.added[0].points[1].u - result.edit.added[0].points[0].u,
                     result.edit.added[0].points[1].v - result.edit.added[0].points[0].v) ==
          Approx(4).margin(kExact));
    CHECK(result.edit.added[1].kind == SketchElementKind::point);
    CHECK(is(result.edit.added[1].points[0], -7, -3));
    const SketchElement& curve = result.edit.added[2];
    REQUIRE(curve.kind == SketchElementKind::spline);
    REQUIRE(curve.points.size() == 3);
    CHECK(is(curve.points[0], -5, 0));
    CHECK(is(curve.points[1], -10, 10));
    CHECK(is(curve.points[2], -20, 5));
}

TEST_CASE("Mirror: an element on the axis is skipped, the axis is never mirrored",
          "[sketch][pattern]") {
    SECTION("a line on the axis, and one beside it") {
        const auto result = mirrorElements(
            {axis(), line("L1", {0, 0}, {0, 20}), line("L2", {5, 0}, {5, 20})}, {"L1", "L2"}, "L9");
        REQUIRE(result.edit.added.size() == 1);
        CHECK(is(result.edit.added[0].points[0], -5, 0));
    }
    SECTION("a circle on the axis") {
        const auto result = mirrorElements({axis(), circle("C1", {0, 0}, 6)}, {"C1"}, "L9");
        CHECK(result.edit.empty());
        CHECK(result.notice == SketchCopyNotice::nothingNew);
    }
    SECTION("the axis in the selection") {
        const auto result = mirrorElements({axis()}, {"L9"}, "L9");
        CHECK(result.edit.empty());
        CHECK(result.notice == SketchCopyNotice::noSelection);
    }
}

TEST_CASE("Mirror: a copy on an existing element is not made twice", "[sketch][pattern]") {
    // a rectangle that is symmetric already: the sides at u = -10 and u = 10
    const auto result =
        mirrorElements({axis(), line("L1", {-10, 0}, {-10, 20}), line("L2", {10, 20}, {10, 0})},
                       {"L1", "L2"}, "L9");
    CHECK(result.edit.empty());
    CHECK(result.notice == SketchCopyNotice::nothingNew);
}

TEST_CASE("Mirror: a helper line stays a helper line", "[sketch][pattern]") {
    const auto result = mirrorElements({axis(), line("L1", {10, 0}, {30, 5}, true)}, {"L1"}, "L9");
    REQUIRE(result.edit.added.size() == 1);
    CHECK(result.edit.added[0].construction);
}

TEST_CASE("Mirror: a helper line as the axis works, a circle or a point does not",
          "[sketch][pattern]") {
    CHECK(mirrorElements({line("L9", {0, -50}, {0, 50}, true), point("P1", {4, 4})}, {"P1"}, "L9")
              .edit.added.size() == 1);
    CHECK(mirrorElements({circle("C1", {0, 0}, 5), point("P1", {4, 4})}, {"P1"}, "C1").notice ==
          SketchCopyNotice::noAxis);
    CHECK(mirrorElements({line("L9", {1, 1}, {1, 1}), point("P1", {4, 4})}, {"P1"}, "L9").notice ==
          SketchCopyNotice::noAxis);
    CHECK(mirrorElements({point("P1", {4, 4})}, {"P1"}, "L7").notice == SketchCopyNotice::noAxis);
}

TEST_CASE("Row: three copies at a distance of 10 are exact", "[sketch][pattern]") {
    // 4 pieces in all (the original and three copies)
    const auto result = linearPattern({line("L1", {0, 0}, {5, 0}), circle("C1", {2, 3}, 1)},
                                      {"L1", "C1"}, {10, 0}, 4);
    REQUIRE(result.notice == SketchCopyNotice::none);
    REQUIRE(result.edit.added.size() == 6);
    for (int k = 1; k <= 3; ++k) {
        const SketchElement& l = result.edit.added[static_cast<std::size_t>((k - 1) * 2)];
        const SketchElement& c = result.edit.added[static_cast<std::size_t>((k - 1) * 2 + 1)];
        CHECK(l.kind == SketchElementKind::line);
        CHECK(is(l.points[0], 10.0 * k, 0));
        CHECK(is(l.points[1], 10.0 * k + 5, 0));
        CHECK(is(c.points[0], 10.0 * k + 2, 3));
    }
}

TEST_CASE("Row: a slanted step moves along both axes", "[sketch][pattern]") {
    const auto result = linearPattern({point("P1", {1, 1})}, {"P1"}, {3, -4}, 3);
    REQUIRE(result.edit.added.size() == 2);
    CHECK(is(result.edit.added[0].points[0], 4, -3));
    CHECK(is(result.edit.added[1].points[0], 7, -7));
}

TEST_CASE("Row: a count of 1, a distance of 0 and more than 200 make nothing",
          "[sketch][pattern]") {
    const std::vector<SketchElement> sketch{point("P1", {1, 1})};
    CHECK(linearPattern(sketch, {"P1"}, {10, 0}, 1).notice == SketchCopyNotice::nothing);
    CHECK(linearPattern(sketch, {"P1"}, {10, 0}, 0).notice == SketchCopyNotice::nothing);
    CHECK(linearPattern(sketch, {"P1"}, {0, 0}, 5).notice == SketchCopyNotice::nothing);
    const auto many = linearPattern(sketch, {"P1"}, {1, 0}, 201);
    CHECK(many.notice == SketchCopyNotice::tooMany);
    CHECK(many.edit.empty());
    CHECK(linearPattern(sketch, {"P9"}, {1, 0}, 3).notice == SketchCopyNotice::noSelection);
}

TEST_CASE("Row: 200 pieces are the most", "[sketch][pattern]") {
    const auto result = linearPattern({point("P1", {0, 0})}, {"P1"}, {1, 0}, 200);
    REQUIRE(result.notice == SketchCopyNotice::none);
    CHECK(result.edit.added.size() == 199);
}

TEST_CASE("Row: a copy that lies on an existing element is not made", "[sketch][pattern]") {
    // the second point is there already
    const auto result =
        linearPattern({point("P1", {0, 0}), point("P2", {10, 0})}, {"P1"}, {10, 0}, 3);
    REQUIRE(result.edit.added.size() == 1);
    CHECK(is(result.edit.added[0].points[0], 20, 0));
}

TEST_CASE("Circle: six pieces round a point, 360 degrees, spread evenly", "[sketch][pattern]") {
    const auto result = circularPattern({point("P1", {10, 0})}, {"P1"}, {0, 0}, 6, 360);
    REQUIRE(result.notice == SketchCopyNotice::none);
    REQUIRE(result.edit.added.size() == 5);
    for (int k = 1; k <= 5; ++k) {
        const double a = k * kPi / 3.0;
        CHECK(is(result.edit.added[static_cast<std::size_t>(k - 1)].points[0], 10 * std::cos(a),
                 10 * std::sin(a)));
    }
}

TEST_CASE("Circle: under 360 degrees the last copy is at the end", "[sketch][pattern]") {
    // 4 pieces over 90 degrees round (5, 5): 30 degrees apart
    const auto result = circularPattern({point("P1", {15, 5})}, {"P1"}, {5, 5}, 4, 90);
    REQUIRE(result.edit.added.size() == 3);
    for (int k = 1; k <= 3; ++k) {
        const double a = k * 30.0 * kPi / 180.0;
        CHECK(is(result.edit.added[static_cast<std::size_t>(k - 1)].points[0], 5 + 10 * std::cos(a),
                 5 + 10 * std::sin(a)));
    }
    CHECK(is(result.edit.added[2].points[0], 5, 15));
}

TEST_CASE("Circle: a negative angle turns clockwise, an arc keeps its turn", "[sketch][pattern]") {
    const auto result = circularPattern({arc("A1", {10, 0}, 3, 0, 90)}, {"A1"}, {0, 0}, 2, -90);
    REQUIRE(result.edit.added.size() == 1);
    const SketchElement& copy = result.edit.added[0];
    CHECK(is(copy.points[0], 0, -10));
    // the start (13, 0) goes to (0, -13), the end (10, 3) to (3, -10)
    CHECK(is(copy.points[1], 0, -13));
    CHECK(is(copy.points[2], 3, -10));
}

TEST_CASE("Circle: a circle at the middle of the pattern lies on itself and is skipped",
          "[sketch][pattern]") {
    const auto result = circularPattern({circle("C1", {0, 0}, 5), point("P1", {5, 0})},
                                        {"C1", "P1"}, {0, 0}, 4, 360);
    REQUIRE(result.edit.added.size() == 3); // only the point
    for (const SketchElement& element : result.edit.added) {
        CHECK(element.kind == SketchElementKind::point);
    }
}

TEST_CASE("Circle: a whole turn of a point that falls on itself", "[sketch][pattern]") {
    // the centre point turned about itself: nothing new
    const auto result = circularPattern({point("P1", {3, 3})}, {"P1"}, {3, 3}, 5, 360);
    CHECK(result.edit.empty());
    CHECK(result.notice == SketchCopyNotice::nothingNew);
}

TEST_CASE("Circle: bad counts and angles make nothing", "[sketch][pattern]") {
    const std::vector<SketchElement> sketch{point("P1", {1, 1})};
    CHECK(circularPattern(sketch, {"P1"}, {0, 0}, 1, 360).notice == SketchCopyNotice::nothing);
    CHECK(circularPattern(sketch, {"P1"}, {0, 0}, 5, 0).notice == SketchCopyNotice::nothing);
    CHECK(circularPattern(sketch, {"P1"}, {0, 0}, 5, 400).notice == SketchCopyNotice::nothing);
    CHECK(circularPattern(sketch, {"P1"}, {0, 0}, 201, 360).notice == SketchCopyNotice::tooMany);
    CHECK(circularPattern(sketch, {"P1"}, {0, 0}, 200, 360).edit.added.size() == 199);
}

TEST_CASE("Pick: a point can be picked, a nearer line wins", "[sketch][pattern]") {
    const std::vector<SketchElement> sketch{line("L1", {0, 0}, {20, 0}), point("P1", {10, 0.5})};
    CHECK(pickElementNear(sketch, {10, 0.4}, 2.0) == "P1");
    CHECK(pickElementNear(sketch, {10, 0.1}, 2.0) == "L1");
    CHECK(pickElementNear(sketch, {10, 10}, 2.0) == std::nullopt);
    CHECK(pickElementNear(sketch, {2, 1}, 2.0) == "L1");
}
