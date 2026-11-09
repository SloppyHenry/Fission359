// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Offsetting chains and rounding or chamfering corners (AP-2.1e2) as pure logic.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "fission/geo/occt/kernel.h"
#include "fission/interaction/sketch_offset.h"

using Catch::Approx;
using namespace fission::interaction;
using fission::core::SketchElement;
using fission::core::SketchElementKind;
using fission::core::SketchPoint;

namespace {

constexpr double kPi = std::numbers::pi;

SketchElement line(std::string id, SketchPoint a, SketchPoint b) {
    return {std::move(id), SketchElementKind::line, {a, b}, false};
}
SketchElement circle(std::string id, SketchPoint middle, double radius) {
    return {
        std::move(id), SketchElementKind::circle, {middle, {middle.u + radius, middle.v}}, false};
}
// counter-clockwise from `from` to `to` degrees
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
SketchElement spline(std::string id, std::vector<SketchPoint> through) {
    return {std::move(id), SketchElementKind::spline, std::move(through), false};
}

// a 40 x 20 rectangle from (0,0), counter-clockwise, ends exactly on each other
std::vector<SketchElement> rectangle() {
    return {line("L1", {0, 0}, {40, 0}), line("L2", {40, 0}, {40, 20}),
            line("L3", {40, 20}, {0, 20}), line("L4", {0, 20}, {0, 0})};
}

// A line that arrives at (10,0) going east and an arc that leaves it going down and to the right:
// a corner that is not smooth (the middle of the arc is (13,4), its radius 5).
std::vector<SketchElement> lineAndArc() {
    const double from = std::atan2(-4.0, -3.0) * 180.0 / kPi;
    return {line("L1", {0, 0}, {10, 0}), arc("A1", {13, 4}, 5, from, from + 90.0)};
}

std::unique_ptr<fission::geo::GeometryKernel> kernel() {
    return fission::geo::occt::makeKernel();
}

struct Box {
    double minU = std::numeric_limits<double>::max();
    double minV = std::numeric_limits<double>::max();
    double maxU = std::numeric_limits<double>::lowest();
    double maxV = std::numeric_limits<double>::lowest();
};

// the box round the end points of the lines
Box boxOf(const std::vector<SketchElement>& elements) {
    Box box;
    for (const SketchElement& element : elements) {
        if (element.kind != SketchElementKind::line) {
            continue;
        }
        for (const SketchPoint& point : element.points) {
            box.minU = std::min(box.minU, point.u);
            box.maxU = std::max(box.maxU, point.u);
            box.minV = std::min(box.minV, point.v);
            box.maxV = std::max(box.maxV, point.v);
        }
    }
    return box;
}

SketchOffsetResult offsetAt(const std::vector<SketchElement>& elements, const std::string& id,
                            const SketchPoint& pointer) {
    const auto chain = findChain(elements, id);
    REQUIRE(chain);
    return offsetChain(*kernel(), *chain, chainSignedDistance(*chain, pointer));
}

void checkPoint(const SketchPoint& point, double u, double v) {
    CHECK(point.u == Approx(u).margin(1e-9));
    CHECK(point.v == Approx(v).margin(1e-9));
}

bool same(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(a.u - b.u, a.v - b.v) < 1e-9;
}

// two elements of lines share an end point
bool joined(const SketchElement& a, const SketchElement& b) {
    return std::ranges::any_of(a.points, [&](const SketchPoint& p) {
        return std::ranges::any_of(b.points, [&](const SketchPoint& q) { return same(p, q); });
    });
}

} // namespace

TEST_CASE("sketch chain: four lines that meet make one closed chain, from any of them",
          "[sketch][offset]") {
    const auto elements = rectangle();
    for (const SketchElement& start : elements) {
        const auto chain = findChain(elements, start.id);
        REQUIRE(chain);
        CHECK(chain->closed);
        CHECK(chain->segments.size() == 4);
        CHECK_FALSE(chain->curve);
    }
}

TEST_CASE("sketch chain: an open chain stops at its ends, a T stops the chain",
          "[sketch][offset]") {
    std::vector<SketchElement> elements{line("L1", {0, 0}, {10, 0}), line("L2", {10, 0}, {10, 10}),
                                        line("L3", {30, 0}, {30, 10})};
    const auto open = findChain(elements, "L2");
    REQUIRE(open);
    CHECK_FALSE(open->closed);
    CHECK(open->segments.size() == 2);
    // a third line on the end of L1: three ends meet there and the chain stops
    elements.push_back(line("L4", {10, 0}, {20, -5}));
    const auto branched = findChain(elements, "L1");
    REQUIRE(branched);
    CHECK(branched->segments.size() == 1);
}

TEST_CASE("sketch chain: a circle is a closed chain of its own, a curve has none",
          "[sketch][offset]") {
    const auto ring = findChain({circle("C1", {0, 0}, 5)}, "C1");
    REQUIRE(ring);
    CHECK(ring->closed);
    CHECK(ring->segments.size() == 1);
    const auto curve = findChain({spline("S1", {{0, 0}, {5, 5}, {10, 0}})}, "S1");
    REQUIRE(curve);
    CHECK(curve->curve);
    CHECK(curve->segments.empty());
    CHECK_FALSE(findChain({}, "L1"));
}

TEST_CASE("sketch offset: a rectangle outwards has joined corners and exact sizes",
          "[sketch][offset]") {
    const auto result = offsetAt(rectangle(), "L1", {20, -5});
    REQUIRE(result.notice == SketchOffsetNotice::none);
    REQUIRE(result.edit.added.size() == 4);
    CHECK(result.edit.removed.empty()); // the original stays
    CHECK(result.edit.moves.empty());
    const Box box = boxOf(result.edit.added);
    CHECK(box.minU == Approx(-5.0).margin(1e-9));
    CHECK(box.maxU == Approx(45.0).margin(1e-9));
    CHECK(box.minV == Approx(-5.0).margin(1e-9));
    CHECK(box.maxV == Approx(25.0).margin(1e-9));
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(joined(result.edit.added[i], result.edit.added[(i + 1) % 4]));
    }
}

TEST_CASE("sketch offset: a rectangle inwards is smaller by the distance on every side",
          "[sketch][offset]") {
    const auto result = offsetAt(rectangle(), "L3", {20, 14});
    REQUIRE(result.notice == SketchOffsetNotice::none);
    REQUIRE(result.edit.added.size() == 4);
    const Box box = boxOf(result.edit.added);
    CHECK(box.minU == Approx(6.0).margin(1e-9));
    CHECK(box.maxU == Approx(34.0).margin(1e-9));
    CHECK(box.minV == Approx(6.0).margin(1e-9));
    CHECK(box.maxV == Approx(14.0).margin(1e-9));
}

TEST_CASE("sketch offset: inwards by more than half the short side crosses itself",
          "[sketch][offset]") {
    const auto chain = findChain(rectangle(), "L1");
    REQUIRE(chain);
    // typed distances can be bigger than the pointer could make; inside is the left (ccw)
    const auto result = offsetChain(*kernel(), *chain, 12.0);
    CHECK(result.notice == SketchOffsetNotice::crossing);
    CHECK(result.edit.empty());
}

TEST_CASE("sketch offset: an open chain keeps its ends square and joins the corner",
          "[sketch][offset]") {
    const std::vector<SketchElement> elements{line("L1", {0, 0}, {30, 0}),
                                              line("L2", {30, 0}, {30, 20})};
    SECTION("outside of the bend: a sharp corner") {
        const auto result = offsetAt(elements, "L1", {10, -5});
        REQUIRE(result.notice == SketchOffsetNotice::none);
        REQUIRE(result.edit.added.size() == 2);
        checkPoint(result.edit.added[0].points[0], 0, -5);
        checkPoint(result.edit.added[0].points[1], 35, -5);
        checkPoint(result.edit.added[1].points[0], 35, -5);
        checkPoint(result.edit.added[1].points[1], 35, 20);
    }
    SECTION("inside of the bend: both are cut where they cross") {
        const auto result = offsetAt(elements, "L1", {10, 5});
        REQUIRE(result.notice == SketchOffsetNotice::none);
        REQUIRE(result.edit.added.size() == 2);
        checkPoint(result.edit.added[0].points[0], 0, 5);
        checkPoint(result.edit.added[0].points[1], 25, 5);
        checkPoint(result.edit.added[1].points[0], 25, 5);
        checkPoint(result.edit.added[1].points[1], 25, 20);
    }
}

TEST_CASE("sketch offset: a line that goes smoothly into an arc stays smooth", "[sketch][offset]") {
    const std::vector<SketchElement> elements{line("L1", {0, 0}, {20, 0}),
                                              arc("A1", {20, 10}, 10, -90, 0)};
    SECTION("towards the middle of the arc the radius gets smaller") {
        const auto result = offsetAt(elements, "L1", {5, 3});
        REQUIRE(result.notice == SketchOffsetNotice::none);
        REQUIRE(result.edit.added.size() == 2);
        checkPoint(result.edit.added[0].points[0], 0, 3);
        checkPoint(result.edit.added[0].points[1], 20, 3);
        const auto& offsetArc = result.edit.added[1];
        REQUIRE(offsetArc.kind == SketchElementKind::arc);
        checkPoint(offsetArc.points[0], 20, 10);
        checkPoint(offsetArc.points[1], 20, 3); // the same place as the end of the line
        checkPoint(offsetArc.points[2], 27, 10);
    }
    SECTION("away from it the radius gets bigger") {
        const auto result = offsetAt(elements, "A1", {33, 10});
        REQUIRE(result.notice == SketchOffsetNotice::none);
        REQUIRE(result.edit.added.size() == 2);
        checkPoint(result.edit.added[0].points[0], 0, -3);
        checkPoint(result.edit.added[0].points[1], 20, -3);
        checkPoint(result.edit.added[1].points[1], 20, -3);
        checkPoint(result.edit.added[1].points[2], 33, 10);
    }
}

TEST_CASE("sketch offset: a convex corner between a line and an arc gets an arc round the corner",
          "[sketch][offset]") {
    const auto result = offsetAt(lineAndArc(), "L1", {3, 2});
    REQUIRE(result.notice == SketchOffsetNotice::none);
    REQUIRE(result.edit.added.size() == 3); // line, arc round the corner, arc
    const auto& round = result.edit.added[1];
    REQUIRE(round.kind == SketchElementKind::arc);
    checkPoint(round.points[0], 10, 0); // round the old corner
    CHECK(std::hypot(round.points[1].u - 10, round.points[1].v) == Approx(2.0).margin(1e-9));
    CHECK(std::hypot(round.points[2].u - 10, round.points[2].v) == Approx(2.0).margin(1e-9));
    // the arc after it has the radius 5 - 2
    CHECK(std::hypot(result.edit.added[2].points[1].u - 13, result.edit.added[2].points[1].v - 4) ==
          Approx(3.0).margin(1e-9));
}

TEST_CASE("sketch offset: a circle changes its radius only", "[sketch][offset]") {
    const std::vector<SketchElement> elements{circle("C1", {5, 5}, 10)};
    SECTION("outwards") {
        const auto result = offsetAt(elements, "C1", {5, 18});
        REQUIRE(result.notice == SketchOffsetNotice::none);
        REQUIRE(result.edit.added.size() == 1);
        CHECK(result.edit.added[0].kind == SketchElementKind::circle);
        checkPoint(result.edit.added[0].points[0], 5, 5);
        CHECK(std::hypot(result.edit.added[0].points[1].u - 5,
                         result.edit.added[0].points[1].v - 5) == Approx(13.0).margin(1e-9));
    }
    SECTION("inwards") {
        const auto result = offsetAt(elements, "C1", {5, 12});
        REQUIRE(result.edit.added.size() == 1);
        CHECK(std::hypot(result.edit.added[0].points[1].u - 5,
                         result.edit.added[0].points[1].v - 5) == Approx(7.0).margin(1e-9));
    }
    SECTION("inwards by the whole radius or more: the distance is bigger than the rounding") {
        const auto chain = findChain(elements, "C1");
        REQUIRE(chain);
        const auto result = offsetChain(*kernel(), *chain, 10.5);
        CHECK(result.notice == SketchOffsetNotice::tooBig);
        CHECK(result.edit.empty());
    }
}

TEST_CASE("sketch offset: an arc towards its middle by more than its radius is refused",
          "[sketch][offset]") {
    const auto chain = findChain({arc("A1", {0, 0}, 10, 0, 180)}, "A1");
    REQUIRE(chain);
    // the arc runs counter-clockwise: the left side is the inside
    CHECK(offsetChain(*kernel(), *chain, 10.5).notice == SketchOffsetNotice::tooBig);
    const auto fine = offsetChain(*kernel(), *chain, 4.0);
    REQUIRE(fine.notice == SketchOffsetNotice::none);
    CHECK(std::hypot(fine.edit.added[0].points[1].u, fine.edit.added[0].points[1].v) ==
          Approx(6.0).margin(1e-9));
}

TEST_CASE("sketch offset: a curve is not offset and says so", "[sketch][offset]") {
    const auto chain = findChain({spline("S1", {{0, 0}, {5, 5}, {10, 0}})}, "S1");
    REQUIRE(chain);
    const auto result = offsetChain(*kernel(), *chain, 2.0);
    CHECK(result.notice == SketchOffsetNotice::curve);
    CHECK(result.edit.empty());
}

TEST_CASE("sketch offset: helper lines only link with helper lines", "[sketch][offset]") {
    auto elements = rectangle();
    elements[1].construction = true;
    const auto chain = findChain(elements, "L1");
    REQUIRE(chain);
    CHECK_FALSE(chain->closed);
    const auto helper = findChain(elements, "L2");
    REQUIRE(helper);
    CHECK(helper->construction);
    const auto made = offsetChain(*kernel(), *helper, 3.0);
    REQUIRE(made.edit.added.size() == 1);
    CHECK(made.edit.added[0].construction);
}

TEST_CASE("sketch offset: the element near a point, curves by their chords", "[sketch][offset]") {
    const std::vector<SketchElement> elements{line("L1", {0, 0}, {40, 0}),
                                              spline("S1", {{0, 20}, {20, 30}, {40, 20}})};
    CHECK(elementNear(elements, {20, 1}, 2).value_or("") == "L1");
    CHECK(elementNear(elements, {10, 25}, 2).value_or("") == "S1");
    CHECK_FALSE(elementNear(elements, {20, 10}, 2));
}

// --- corners ------------------------------------------------------------------------------

TEST_CASE("sketch corner: a corner is where exactly two ends meet", "[sketch][corner]") {
    auto elements = rectangle();
    const auto corner = findCorner(elements, {39.9, 0.1}, 1.0);
    REQUIRE(corner);
    checkPoint(corner->at, 40, 0);
    CHECK_FALSE(corner->curve);
    CHECK_FALSE(findCorner(elements, {20, 10}, 1.0));
    // a third end on the corner: no clear corner there any more
    elements.push_back(line("L5", {40, 0}, {50, -5}));
    CHECK_FALSE(findCorner(elements, {40, 0}, 1.0));
}

TEST_CASE("sketch corner: the corner of a curve is marked, nothing is rounded",
          "[sketch][corner]") {
    const std::vector<SketchElement> elements{line("L1", {0, 0}, {10, 0}),
                                              spline("S1", {{10, 0}, {15, 5}, {20, 0}})};
    const auto corner = findCorner(elements, {10, 0}, 1.0);
    REQUIRE(corner);
    CHECK(corner->curve);
    const auto result = roundCorner(elements, *corner, 1.0);
    CHECK(result.notice == SketchCornerNotice::curve);
    CHECK(result.edit.empty());
}

TEST_CASE("sketch corner: rounding a corner of a rectangle is exact and tangent",
          "[sketch][corner]") {
    const auto elements = rectangle();
    const auto corner = findCorner(elements, {40, 0}, 1.0);
    REQUIRE(corner);
    const auto result = roundCorner(elements, *corner, 5.0);
    REQUIRE(result.notice == SketchCornerNotice::none);
    REQUIRE(result.edit.moves.size() == 2);
    REQUIRE(result.edit.added.size() == 1);
    const SketchElement& rounding = result.edit.added[0];
    REQUIRE(rounding.kind == SketchElementKind::arc);
    const SketchPoint centre = rounding.points[0];
    checkPoint(centre, 35, 5);
    checkPoint(rounding.points[1], 35, 0); // counter-clockwise: from the bottom side ...
    checkPoint(rounding.points[2], 40, 5); // ... to the right side
    // the middle of the arc, 45 degrees from the start
    const double half =
        std::atan2(rounding.points[1].v - centre.v, rounding.points[1].u - centre.u) + kPi / 4.0;
    CHECK(centre.u + 5.0 * std::cos(half) == Approx(35.0 + 5.0 * std::sqrt(0.5)).margin(1e-9));
    CHECK(centre.v + 5.0 * std::sin(half) == Approx(5.0 - 5.0 * std::sqrt(0.5)).margin(1e-9));
    // the two sides end at the tangent points: the radius there is perpendicular to the side
    REQUIRE(result.edit.moves[0].element == "L1");
    REQUIRE(result.edit.moves[1].element == "L2");
    checkPoint(result.edit.moves[0].points[0], 0, 0);
    checkPoint(result.edit.moves[0].points[1], 35, 0);
    checkPoint(result.edit.moves[1].points[0], 40, 5);
    checkPoint(result.edit.moves[1].points[1], 40, 20);
    const auto tangent = [&](const SketchPoint& from, const SketchPoint& to) {
        const double dx = to.u - from.u;
        const double dy = to.v - from.v;
        return (dx * (to.u - centre.u) + dy * (to.v - centre.v)) / std::hypot(dx, dy);
    };
    CHECK(tangent(result.edit.moves[0].points[0], result.edit.moves[0].points[1]) ==
          Approx(0.0).margin(1e-9));
    CHECK(tangent(result.edit.moves[1].points[1], result.edit.moves[1].points[0]) ==
          Approx(0.0).margin(1e-9));
}

TEST_CASE("sketch corner: chamfering cuts the same distance from the corner on both sides",
          "[sketch][corner]") {
    const auto elements = rectangle();
    const auto corner = findCorner(elements, {40, 20}, 1.0);
    REQUIRE(corner);
    const auto result = chamferCorner(elements, *corner, 4.0);
    REQUIRE(result.notice == SketchCornerNotice::none);
    REQUIRE(result.edit.added.size() == 1);
    REQUIRE(result.edit.moves.size() == 2);
    const SketchElement& cut = result.edit.added[0];
    REQUIRE(cut.kind == SketchElementKind::line);
    const bool order = cut.points[0].u == Approx(40.0).margin(1e-9);
    checkPoint(order ? cut.points[0] : cut.points[1], 40, 16);
    checkPoint(order ? cut.points[1] : cut.points[0], 36, 20);
    // the sides are shortened to the ends of the cut
    CHECK(result.edit.moves[0].element == "L2");
    checkPoint(result.edit.moves[0].points[1], 40, 16);
    CHECK(result.edit.moves[1].element == "L3");
    checkPoint(result.edit.moves[1].points[0], 36, 20);
}

TEST_CASE("sketch corner: a radius too big for the corner says how big it can be",
          "[sketch][corner]") {
    const auto elements = rectangle();
    const auto corner = findCorner(elements, {40, 0}, 1.0);
    REQUIRE(corner);
    const auto result = roundCorner(elements, *corner, 25.0);
    CHECK(result.notice == SketchCornerNotice::tooBig);
    CHECK(result.edit.empty());
    // the short side is 20 mm and a rest of 0.01 mm must stay
    CHECK(result.limit == Approx(19.99).margin(1e-9));
    // and that radius works
    CHECK(roundCorner(elements, *corner, result.limit).notice == SketchCornerNotice::none);
    const auto chamfer = chamferCorner(elements, *corner, 25.0);
    CHECK(chamfer.notice == SketchCornerNotice::tooBig);
    CHECK(chamfer.limit == Approx(19.99).margin(1e-9));
}

TEST_CASE("sketch corner: too small is nothing, a straight join is no corner", "[sketch][corner]") {
    const auto elements = rectangle();
    const auto corner = findCorner(elements, {40, 0}, 1.0);
    REQUIRE(corner);
    CHECK(roundCorner(elements, *corner, 0.001).notice == SketchCornerNotice::tooSmall);
    CHECK(chamferCorner(elements, *corner, 0.0).notice == SketchCornerNotice::tooSmall);
    const std::vector<SketchElement> straight{line("L1", {0, 0}, {10, 0}),
                                              line("L2", {10, 0}, {20, 0})};
    const auto join = findCorner(straight, {10, 0}, 1.0);
    REQUIRE(join);
    CHECK(roundCorner(straight, *join, 2.0).notice == SketchCornerNotice::noCorner);
    CHECK(chamferCorner(straight, *join, 2.0).notice == SketchCornerNotice::noCorner);
}

TEST_CASE("sketch corner: a corner of a line and an arc is rounded tangent to both",
          "[sketch][corner]") {
    const SketchPoint arcCentre{13, 4};
    const auto elements = lineAndArc();
    const auto corner = findCorner(elements, {10, 0}, 1.0);
    REQUIRE(corner);
    const auto result = roundCorner(elements, *corner, 1.0);
    REQUIRE(result.notice == SketchCornerNotice::none);
    REQUIRE(result.edit.added.size() == 1);
    REQUIRE(result.edit.moves.size() == 2);
    const SketchElement& rounding = result.edit.added[0];
    const SketchPoint c = rounding.points[0];
    CHECK(std::hypot(rounding.points[1].u - c.u, rounding.points[1].v - c.v) ==
          Approx(1.0).margin(1e-9));
    // tangent to the line y = 0, on the side where the arc goes (down)
    CHECK(c.v == Approx(-1.0).margin(1e-9));
    // tangent to the arc: the centres are 5 + 1 or 5 - 1 apart
    const double apart = std::hypot(c.u - arcCentre.u, c.v - arcCentre.v);
    CHECK((std::abs(apart - 6.0) < 1e-9 || std::abs(apart - 4.0) < 1e-9));
    // the arc's own end moved onto the circle, and the line's end to the foot of the centre
    for (const auto& move : result.edit.moves) {
        if (move.element == "A1") {
            CHECK(std::hypot(move.points[1].u - arcCentre.u, move.points[1].v - arcCentre.v) ==
                  Approx(5.0).margin(1e-9));
        } else {
            CHECK(move.points[1].v == Approx(0.0).margin(1e-12));
            CHECK(move.points[1].u == Approx(c.u).margin(1e-9));
        }
    }
}

TEST_CASE("sketch corner: a pointer says a radius or a distance", "[sketch][corner]") {
    const auto elements = rectangle();
    const auto corner = findCorner(elements, {40, 0}, 1.0);
    REQUIRE(corner);
    // the middle of the arc follows the pointer: at (35,5) the radius is 5
    CHECK(cornerRadiusAt(elements, *corner, {35, 5}) == Approx(5.0).margin(1e-9));
    // on the bisector, 4 mm from each side, the chamfer distance is 4
    CHECK(cornerDistanceAt(elements, *corner, {38, 2}) == Approx(4.0).margin(1e-9));
    CHECK(cornerDistanceAt(elements, *corner, {45, -5}) == Approx(0.0).margin(1e-9));
}
