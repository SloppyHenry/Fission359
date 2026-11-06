// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The pointer on a sketch plane (AP-2.1b): the point where a ray meets the plane, and what the
// pointer catches on.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "fission/interaction/sketch_snap.h"

using Catch::Approx;
using namespace fission;
using namespace fission::interaction;
using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;

namespace {

SketchElement element(const char* id, SketchElementKind kind, std::vector<SketchPoint> points,
                      bool construction = false) {
    SketchElement made;
    made.id = id;
    made.kind = kind;
    made.points = std::move(points);
    made.construction = construction;
    return made;
}

// Zoom 2 px/mm and a radius of 10 px: it reaches 5 mm. No grid unless a test asks for one.
SketchSnapSettings settings(double grid = 0.0) {
    SketchSnapSettings made;
    made.radiusPixels = 10.0;
    made.pixelsPerMillimetre = 2.0;
    made.gridStep = grid;
    return made;
}

} // namespace

// --- the pointer on the plane ----------------------------------------------------------------

TEST_CASE("planePoint: a ray from above meets the ground plane under it", "[sketch][snap]") {
    const core::SketchPlane ground;
    const auto at = planePoint({{12.0, -7.0, 100.0}, {0.0, 0.0, -1.0}}, ground);
    REQUIRE(at.has_value());
    CHECK(at->u == Approx(12.0));
    CHECK(at->v == Approx(-7.0));
}

TEST_CASE("planePoint: a slanted ray gives the point where it crosses, whatever its length",
          "[sketch][snap]") {
    const core::SketchPlane ground;
    // From (0, 0, 10) towards (+x, +y, -z): after 10 mm of height it is 10 mm out in both.
    const auto at = planePoint({{0.0, 0.0, 10.0}, {3.0, 3.0, -3.0}}, ground);
    REQUIRE(at.has_value());
    CHECK(at->u == Approx(10.0));
    CHECK(at->v == Approx(10.0));
}

TEST_CASE("planePoint: a ray parallel to the plane meets nothing", "[sketch][snap]") {
    const core::SketchPlane ground;
    CHECK_FALSE(planePoint({{0.0, 0.0, 5.0}, {1.0, 0.0, 0.0}}, ground));
    CHECK_FALSE(planePoint({{0.0, 0.0, 5.0}, {0.0, 0.0, 0.0}}, ground));
}

TEST_CASE("planePoint: a plane behind the eye meets nothing", "[sketch][snap]") {
    const core::SketchPlane ground;
    CHECK_FALSE(planePoint({{0.0, 0.0, 5.0}, {0.0, 0.0, 1.0}}, ground));
}

TEST_CASE("planePoint: works in the coordinates of a plane that is moved and turned",
          "[sketch][snap]") {
    // A wall facing the viewer at y = 20: u = X, v = Z, origin (100, 20, 0).
    const core::SketchPlane wall({100.0, 20.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
    const auto at = planePoint({{110.0, -50.0, 30.0}, {0.0, 1.0, 0.0}}, wall);
    REQUIRE(at.has_value());
    CHECK(at->u == Approx(10.0));
    CHECK(at->v == Approx(30.0));
}

// --- what the pointer catches on ------------------------------------------------------------

TEST_CASE("sketchSnap: catches on the end of a line within the radius", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("L1", SketchElementKind::line, {{0.0, 0.0}, {40.0, 0.0}})};
    const SketchSnap near = sketchSnap({41.5, 1.0}, sketch, settings());
    CHECK(near.kind == SketchSnapKind::point);
    CHECK(near.position == SketchPoint{40.0, 0.0});
    CHECK(near.element == "L1");
    // The radius is in pixels: at 4 px/mm it only reaches 2.5 mm, 5 mm away is out.
    SketchSnapSettings zoomed = settings();
    zoomed.pixelsPerMillimetre = 4.0;
    CHECK(sketchSnap({43.0, 4.0}, sketch, zoomed).kind == SketchSnapKind::none);
    CHECK(sketchSnap({43.0, 4.0}, sketch, settings()).kind == SketchSnapKind::point);
    CHECK(sketchSnap({42.0, 0.0}, sketch, zoomed).kind == SketchSnapKind::point);
}

TEST_CASE("sketchSnap: catches on a single point and on the points of a spline", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("P1", SketchElementKind::point, {{5.0, 5.0}}),
        element("S1", SketchElementKind::spline, {{20.0, 0.0}, {30.0, 10.0}, {40.0, 0.0}})};
    CHECK(sketchSnap({6.0, 4.0}, sketch, settings()).element == "P1");
    const SketchSnap onSpline = sketchSnap({31.0, 9.0}, sketch, settings());
    CHECK(onSpline.kind == SketchSnapKind::point);
    CHECK(onSpline.position == SketchPoint{30.0, 10.0});
}

TEST_CASE("sketchSnap: catches on the middle of a line", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("L1", SketchElementKind::line, {{0.0, 0.0}, {40.0, 20.0}})};
    const SketchSnap snap = sketchSnap({21.0, 9.0}, sketch, settings());
    CHECK(snap.kind == SketchSnapKind::middle);
    CHECK(snap.position == SketchPoint{20.0, 10.0});
    CHECK(snap.element == "L1");
}

TEST_CASE("sketchSnap: catches on the centre of a circle and of an arc", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("C1", SketchElementKind::circle, {{0.0, 0.0}, {10.0, 0.0}}),
        element("A1", SketchElementKind::arc, {{50.0, 0.0}, {60.0, 0.0}, {50.0, 10.0}})};
    const SketchSnap circle = sketchSnap({1.0, -2.0}, sketch, settings());
    CHECK(circle.kind == SketchSnapKind::middle);
    CHECK(circle.position == SketchPoint{0.0, 0.0});
    CHECK(circle.element == "C1");
    const SketchSnap arc = sketchSnap({49.0, 1.0}, sketch, settings());
    CHECK(arc.kind == SketchSnapKind::middle);
    CHECK(arc.element == "A1");
}

TEST_CASE("sketchSnap: catches on both ends of an arc, the second one on the circle",
          "[sketch][snap]") {
    // From (60, 0) counter-clockwise to the direction of (50, 20): its end is (50, 10).
    const std::vector<SketchElement> sketch{
        element("A1", SketchElementKind::arc, {{50.0, 0.0}, {60.0, 0.0}, {50.0, 20.0}})};
    const SketchSnap end = sketchSnap({50.5, 11.0}, sketch, settings());
    CHECK(end.kind == SketchSnapKind::point);
    CHECK(end.position.u == Approx(50.0));
    CHECK(end.position.v == Approx(10.0));
    CHECK(sketchSnap({61.0, 1.0}, sketch, settings()).position == SketchPoint{60.0, 0.0});
}

TEST_CASE("sketchSnap: lines up horizontally with a point and gives a guide", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("P1", SketchElementKind::point, {{10.0, 20.0}})};
    const SketchSnap snap = sketchSnap({60.0, 22.0}, sketch, settings());
    CHECK(snap.kind == SketchSnapKind::alignment);
    CHECK(snap.position == SketchPoint{60.0, 20.0});
    REQUIRE(snap.guides.size() == 1);
    CHECK(snap.guides[0].from == SketchPoint{10.0, 20.0});
    CHECK(snap.guides[0].to == SketchPoint{60.0, 20.0});
}

TEST_CASE("sketchSnap: lines up vertically, and both ways at once gives two guides",
          "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("P1", SketchElementKind::point, {{10.0, 20.0}}),
        element("P2", SketchElementKind::point, {{70.0, 50.0}})};
    const SketchSnap vertical = sketchSnap({12.0, 90.0}, sketch, settings());
    CHECK(vertical.kind == SketchSnapKind::alignment);
    CHECK(vertical.position == SketchPoint{10.0, 90.0});
    REQUIRE(vertical.guides.size() == 1);

    // Level with P1 (v = 20) and above P2 (u = 70): the corner of the two.
    const SketchSnap both = sketchSnap({71.0, 21.0}, sketch, settings());
    CHECK(both.kind == SketchSnapKind::alignment);
    CHECK(both.position == SketchPoint{70.0, 20.0});
    CHECK(both.guides.size() == 2);
}

TEST_CASE("sketchSnap: lines up with the middle of a line as well", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("L1", SketchElementKind::line, {{0.0, 0.0}, {20.0, 0.0}})};
    const SketchSnap snap = sketchSnap({10.0, 40.0}, sketch, settings());
    CHECK(snap.kind == SketchSnapKind::alignment);
    CHECK(snap.position == SketchPoint{10.0, 40.0});
    REQUIRE(snap.guides.size() == 1);
    CHECK(snap.guides[0].from == SketchPoint{10.0, 0.0});
}

TEST_CASE("sketchSnap: with only a grid the position goes to the nearest grid point",
          "[sketch][snap]") {
    const SketchSnap snap = sketchSnap({2.3, -4.6}, {}, settings(1.0));
    CHECK(snap.kind == SketchSnapKind::grid);
    CHECK(snap.position == SketchPoint{2.0, -5.0});
    const SketchSnap coarse = sketchSnap({26.0, 14.0}, {}, settings(10.0));
    CHECK(coarse.position == SketchPoint{30.0, 10.0});
    // Without a grid and without anything to catch on, the position stays as it is.
    const SketchSnap free = sketchSnap({2.3, -4.6}, {}, settings(0.0));
    CHECK(free.kind == SketchSnapKind::none);
    CHECK(free.position == SketchPoint{2.3, -4.6});
}

TEST_CASE("sketchSnap: a way that does not line up goes to the grid", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("P1", SketchElementKind::point, {{10.5, 20.5}})};
    const SketchSnap snap = sketchSnap({60.3, 22.0}, sketch, settings(1.0));
    CHECK(snap.kind == SketchSnapKind::alignment);
    CHECK(snap.position.u == Approx(60.0));
    CHECK(snap.position.v == Approx(20.5));
}

TEST_CASE("sketchSnap: precedence is point, then middle, then alignment, then grid",
          "[sketch][snap]") {
    // A line from (0, 0) to (20, 0): ends and middle (10, 0). Another point at (10, 3) is within
    // reach of the middle as well.
    const std::vector<SketchElement> sketch{
        element("L1", SketchElementKind::line, {{0.0, 0.0}, {20.0, 0.0}}),
        element("P1", SketchElementKind::point, {{10.0, 3.0}})};
    // Between the middle and the point, nearer to the middle: the point still wins.
    CHECK(sketchSnap({10.0, 1.0}, sketch, settings(1.0)).kind == SketchSnapKind::point);
    // Only the middle in reach (the point is 6 mm away): the middle beats alignment and grid.
    const SketchSnap middle = sketchSnap({10.0, -3.0}, sketch, settings(1.0));
    CHECK(middle.kind == SketchSnapKind::middle);
    CHECK(middle.position == SketchPoint{10.0, 0.0});
    // Out of reach of every point and middle, but level with P1 and above the line's end: the
    // alignment beats the grid.
    const SketchSnap lined = sketchSnap({3.0, 5.5}, sketch, settings(1.0));
    CHECK(lined.kind == SketchSnapKind::alignment);
    CHECK(lined.position.u == Approx(0.0).margin(1e-9));
    CHECK(lined.position.v == Approx(3.0));
    CHECK(lined.guides.size() == 2);
    // Far from everything: the grid.
    CHECK(sketchSnap({50.4, 30.6}, sketch, settings(1.0)).kind == SketchSnapKind::grid);
}

TEST_CASE("sketchSnap: construction lines count, and switching snapping off keeps the position",
          "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("L1", SketchElementKind::line, {{0.0, 0.0}, {40.0, 0.0}}, true)};
    CHECK(sketchSnap({39.0, 1.0}, sketch, settings()).kind == SketchSnapKind::point);
    SketchSnapSettings off = settings(1.0);
    off.enabled = false;
    const SketchSnap free = sketchSnap({39.3, 1.4}, sketch, off);
    CHECK(free.kind == SketchSnapKind::none);
    CHECK(free.position == SketchPoint{39.3, 1.4});
    CHECK(free.guides.empty());
}

TEST_CASE("sketchSnap: ignores elements with unusable points", "[sketch][snap]") {
    const std::vector<SketchElement> sketch{
        element("L1", SketchElementKind::line, {{0.0, 0.0}})}; // a line needs two points
    CHECK(sketchSnap({0.5, 0.5}, sketch, settings()).kind == SketchSnapKind::none);
}

TEST_CASE("sketchGridStep: the grid of the bed, coarser where lines would crowd",
          "[sketch][snap]") {
    // 1 mm at 8 px/mm is 8 px apart: fine as it is.
    CHECK(sketchGridStep(1.0, 8.0) == Approx(1.0));
    // At 2 px/mm 1 mm is 2 px: 2 mm is 4 px, 5 mm is 10 px.
    CHECK(sketchGridStep(1.0, 2.0) == Approx(5.0));
    // At 0.5 px/mm: 10 mm is 5 px, 20 mm is 10 px.
    CHECK(sketchGridStep(1.0, 0.5) == Approx(20.0));
    // A finer grid setting is kept while there is room for it.
    CHECK(sketchGridStep(0.1, 100.0) == Approx(0.1));
    CHECK(sketchGridStep(0.1, 20.0) == Approx(0.5)); // 0.1 is 2 px, 0.2 is 4, 0.5 is 10
    CHECK(sketchGridStep(0.0, 8.0) == 0.0);
    CHECK(sketchGridStep(1.0, 0.0) == 0.0);
}
