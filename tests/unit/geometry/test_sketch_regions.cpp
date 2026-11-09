// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The closed areas of a sketch (AP-2.1g): golden areas, holes, overlaps, open chains, helper
// curves left out, and the time for a sketch of 100 elements.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
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

SketchCurve line(Point2d a, Point2d b) {
    return {SketchCurveKind::line, {a, b}};
}
SketchCurve circle(Point2d middle, double radius) {
    return {SketchCurveKind::circle, {middle, {middle.x + radius, middle.y}}};
}
SketchCurve arc(Point2d middle, double radius, double from, double to) {
    const double a = from * kPi / 180.0;
    const double b = to * kPi / 180.0;
    return {SketchCurveKind::arc,
            {middle,
             {middle.x + radius * std::cos(a), middle.y + radius * std::sin(a)},
             {middle.x + radius * std::cos(b), middle.y + radius * std::sin(b)}}};
}
std::vector<SketchCurve> rectangle(double x0, double y0, double x1, double y1) {
    return {line({x0, y0}, {x1, y0}), line({x1, y0}, {x1, y1}), line({x1, y1}, {x0, y1}),
            line({x0, y1}, {x0, y0})};
}
std::vector<SketchCurve> join(std::vector<SketchCurve> a, const std::vector<SketchCurve>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
std::vector<double> areas(const SketchRegionResult& result) {
    std::vector<double> out;
    for (const SketchRegion& region : result.regions) {
        out.push_back(region.area);
    }
    return out;
}

// Area of the lens of two circles of radius r whose middles are d apart.
double lens(double r, double d) {
    return 2.0 * r * r * std::acos(d / (2.0 * r)) - 0.5 * d * std::sqrt(4.0 * r * r - d * d);
}

double polygonArea(const std::vector<Point2d>& p) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 1 < p.size(); ++i) {
        sum += p[i].x * p[i + 1].y - p[i + 1].x * p[i].y;
    }
    return 0.5 * sum;
}

} // namespace

TEST_CASE("sketch regions: two overlapping circles make three regions", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const double r = 10.0;
    const double d = 10.0;
    const auto result = k->sketchRegions({circle({0, 0}, r), circle({d, 0}, r)});
    REQUIRE(result.regions.size() == 3);
    CHECK(result.rejected == 0);
    const double l = lens(r, d);
    const double rest = kPi * r * r - l;
    const std::vector<double> a = areas(result);
    CHECK(a[0] == Approx(l).epsilon(1e-9).margin(1e-6));
    CHECK(a[1] == Approx(rest).epsilon(1e-9).margin(1e-6));
    CHECK(a[2] == Approx(rest).epsilon(1e-9).margin(1e-6));
    // the lens lies in the middle, on the left of both curves
    const SketchRegion& middle = result.regions[0];
    CHECK(middle.centroid.x == Approx(5.0).margin(1e-6));
    CHECK(middle.centroid.y == Approx(0.0).margin(1e-6));
    REQUIRE(middle.outer.pieces.size() == 2);
    for (const SketchRegionPiece& piece : middle.outer.pieces) {
        REQUIRE(piece.sides.size() == 1);
        CHECK(piece.sides[0].left);
    }
    CHECK(middle.holes.empty());
    CHECK(polygonArea(middle.outer.polygon) > 0.0);
    CHECK(polygonArea(middle.outer.polygon) == Approx(l).margin(0.005 * 50.0)); // sag of the chords
}

TEST_CASE("sketch regions: a circle inside a rectangle makes a ring and a disc",
          "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const auto result = k->sketchRegions(join(rectangle(0, 0, 40, 30), {circle({20, 15}, 5)}));
    REQUIRE(result.regions.size() == 2);
    CHECK(result.regions[0].area == Approx(kPi * 25.0).epsilon(1e-9));
    CHECK(result.regions[0].holes.empty());
    CHECK(result.regions[1].area == Approx(1200.0 - kPi * 25.0).epsilon(1e-9));
    REQUIRE(result.regions[1].holes.size() == 1);
    CHECK(polygonArea(result.regions[1].holes[0].polygon) < 0.0);
    // the shading: triangles that add up to the area (the hole is left out)
    for (const SketchRegion& region : result.regions) {
        REQUIRE(region.triangles.size() >= 3);
        REQUIRE(region.triangles.size() % 3 == 0);
        double sum = 0.0;
        for (std::size_t i = 0; i < region.triangles.size(); i += 3) {
            const std::vector<Point2d> t{region.triangles[i], region.triangles[i + 1],
                                         region.triangles[i + 2], region.triangles[i]};
            sum += std::abs(polygonArea(t));
        }
        CHECK(sum == Approx(region.area).epsilon(2e-3));
    }
    // the point inside the ring is not in the hole
    const Point2d inside = result.regions[1].inside;
    CHECK(std::hypot(inside.x - 20.0, inside.y - 15.0) > 5.0);
    CHECK(inside.x > 0.0);
    CHECK(inside.x < 40.0);
}

TEST_CASE("sketch regions: a circle over the edge of a rectangle makes three regions",
          "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const double r = 5.0;
    const auto result = k->sketchRegions(join(rectangle(0, 0, 40, 30), {circle({40, 15}, r)}));
    REQUIRE(result.regions.size() == 3);
    const double half = kPi * r * r / 2.0; // the middle of the circle lies on the edge
    const std::vector<double> a = areas(result);
    CHECK(a[0] == Approx(half).epsilon(1e-9));
    CHECK(a[1] == Approx(half).epsilon(1e-9));
    CHECK(a[2] == Approx(1200.0 - half).epsilon(1e-9));
}

TEST_CASE("sketch regions: an open chain of lines makes none", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const auto result =
        k->sketchRegions({line({0, 0}, {10, 0}), line({10, 0}, {10, 10}), line({10, 10}, {0, 10})});
    CHECK(result.regions.empty());
    CHECK(result.rejected == 0);
    CHECK(k->sketchRegions({}).regions.empty());
}

TEST_CASE("sketch regions: an L profile is one region", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const std::vector<SketchCurve> l{line({0, 0}, {30, 0}),    line({30, 0}, {30, 10}),
                                     line({30, 10}, {10, 10}), line({10, 10}, {10, 40}),
                                     line({10, 40}, {0, 40}),  line({0, 40}, {0, 0})};
    const auto result = k->sketchRegions(l);
    REQUIRE(result.regions.size() == 1);
    CHECK(result.regions[0].area == Approx(30.0 * 10.0 + 10.0 * 30.0).epsilon(1e-12));
    CHECK(result.regions[0].outer.pieces.size() == 6);
    // the point inside is really inside (the centroid of an L is near the corner)
    const Point2d p = result.regions[0].inside;
    const bool inLower = p.x > 0 && p.x < 30 && p.y > 0 && p.y < 10;
    const bool inUpper = p.x > 0 && p.x < 10 && p.y > 0 && p.y < 40;
    CHECK((inLower || inUpper));
}

TEST_CASE("sketch regions: a rectangle with a rounded corner has the exact area",
          "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const double r = 5.0;
    const std::vector<SketchCurve> shape{line({0, 0}, {40, 0}), line({40, 0}, {40, 25}),
                                         arc({35, 25}, r, 0, 90), // the rounded corner at (40, 30)
                                         line({35, 30}, {0, 30}), line({0, 30}, {0, 0})};
    const auto result = k->sketchRegions(shape);
    REQUIRE(result.regions.size() == 1);
    CHECK(result.regions[0].area == Approx(1200.0 - r * r + kPi * r * r / 4.0).epsilon(1e-10));
    // the polygon stays within the deflection of the arc (rule 12)
    const auto& polygon = result.regions[0].outer.polygon;
    double worst = 0.0;
    for (const Point2d& p : polygon) {
        if (p.x > 35.0 + 1e-9 && p.y > 25.0 + 1e-9) {
            worst = std::max(worst, std::abs(std::hypot(p.x - 35.0, p.y - 25.0) - r));
        }
    }
    CHECK(worst <= 1e-6); // the points lie on the arc; chords sag by at most the deflection
    CHECK(polygonArea(polygon) == Approx(result.regions[0].area).epsilon(1e-4));
}

TEST_CASE("sketch regions: a spline closes a region", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    SketchCurve spline{SketchCurveKind::spline, {{0, 0}, {10, 5}, {20, 0}, {10, -5}, {0, 0}}};
    const auto result = k->sketchRegions({spline});
    REQUIRE(result.regions.size() == 1);
    CHECK(result.regions[0].area > 50.0);
    CHECK(result.regions[0].area < 200.0);
    CHECK(result.rejected == 0);
}

TEST_CASE("sketch regions: a line through a rectangle splits it, a spur does not",
          "[sketch][regions]") {
    const auto k = occt::makeKernel();
    const auto split = k->sketchRegions(join(rectangle(0, 0, 40, 30), {line({20, -5}, {20, 35})}));
    REQUIRE(split.regions.size() == 2);
    CHECK(split.regions[0].area == Approx(600.0).epsilon(1e-12));
    CHECK(split.regions[1].area == Approx(600.0).epsilon(1e-12));
    // a loose line inside the rectangle changes nothing
    const auto spur = k->sketchRegions(join(rectangle(0, 0, 40, 30), {line({10, 10}, {20, 10})}));
    INFO("rejected " << spur.rejected);
    REQUIRE(spur.regions.size() == 1);
    CHECK(spur.regions[0].area == Approx(1200.0).epsilon(1e-12));
    // a loose line touching the edge from the inside neither
    const auto touching =
        k->sketchRegions(join(rectangle(0, 0, 40, 30), {line({0, 10}, {20, 10})}));
    REQUIRE(touching.regions.size() == 1);
    CHECK(touching.regions[0].area == Approx(1200.0).epsilon(1e-12));
}

TEST_CASE("sketch regions: curves on top of each other count for both", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    auto curves = rectangle(0, 0, 10, 10);
    curves.push_back(line({0, 0}, {10, 0})); // on the first edge
    const auto result = k->sketchRegions(curves);
    REQUIRE(result.regions.size() == 1);
    CHECK(result.regions[0].area == Approx(100.0).epsilon(1e-12));
    std::size_t both = 0;
    for (const SketchRegionPiece& piece : result.regions[0].outer.pieces) {
        both += piece.sides.size() == 2 ? 1 : 0;
    }
    CHECK(both == 1);
}

TEST_CASE("sketch regions: bad curves are an error", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    CHECK_THROWS_AS(k->sketchRegions({line({0, 0}, {0, 0})}), GeometryError);
    CHECK_THROWS_AS(k->sketchRegions({SketchCurve{SketchCurveKind::circle, {{0, 0}}}}),
                    GeometryError);
    CHECK_THROWS_AS(k->sketchRegions({line({0, 0}, {std::nan(""), 1})}), GeometryError);
}

TEST_CASE("sketch regions: 100 elements take a moment", "[sketch][regions]") {
    const auto k = occt::makeKernel();
    std::vector<SketchCurve> curves; // a 10 x 5 field of small squares plus 50 circles
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 5; ++j) {
            curves.push_back(circle({i * 12.0 + 6.0, j * 12.0 + 6.0}, 4.0 + 0.1 * (i % 3)));
        }
    }
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 10; ++j) {
            curves.push_back(line({j * 12.0 + 3.0, i * 12.0}, {j * 12.0 + 9.0, i * 12.0 + 12.0}));
        }
    }
    REQUIRE(curves.size() == 100);
    const auto begin = std::chrono::steady_clock::now();
    const auto result = k->sketchRegions(curves);
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin);
    std::cout << "sketch regions: 100 elements -> " << result.regions.size() << " regions in "
              << ms.count() << " ms (budget 200 ms, a hint, not a test)\n";
    CHECK(!result.regions.empty());
    CHECK(result.rejected == 0);
}
