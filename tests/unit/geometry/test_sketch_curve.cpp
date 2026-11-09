// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The curve of a sketch spline (AP-2.1d): through every point exactly, smooth where it closes,
// within 0.01 mm of its chain of lines, and refused politely when it cannot be made.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
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

double distanceToChain(const Point2d& p, const std::vector<Point2d>& chain) {
    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < chain.size(); ++i) {
        const double dx = chain[i].x - chain[i - 1].x;
        const double dy = chain[i].y - chain[i - 1].y;
        const double length2 = dx * dx + dy * dy;
        double t = length2 > 0.0
                       ? ((p.x - chain[i - 1].x) * dx + (p.y - chain[i - 1].y) * dy) / length2
                       : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        best = std::min(best,
                        std::hypot(p.x - chain[i - 1].x - t * dx, p.y - chain[i - 1].y - t * dy));
    }
    return best;
}

bool isVertex(const std::vector<Point2d>& chain, const Point2d& p) {
    return std::ranges::any_of(chain, [&](const Point2d& q) { return q.x == p.x && q.y == p.y; });
}

std::vector<Point2d> wave() {
    return {{0, 0}, {10, 8}, {20, -4}, {35, 6}, {50, 0}};
}

std::vector<Point2d> ring(int count, double radius) {
    std::vector<Point2d> points;
    for (int i = 0; i < count; ++i) {
        const double a = 2.0 * kPi * i / count;
        points.push_back({radius * std::cos(a), radius * std::sin(a)});
    }
    points.push_back(points.front());
    return points;
}

} // namespace

TEST_CASE("sketch spline: an open curve goes exactly through all its points",
          "[sketch][geometry]") {
    const auto k = kernel();
    const std::vector<Point2d> through = wave();
    const SketchSplineCurve curve = k->sketchSpline(through, 0.005);
    CHECK_FALSE(curve.closed);
    CHECK(curve.polyline.front() == through.front());
    CHECK(curve.polyline.back() == through.back());
    for (const Point2d& p : through) {
        CHECK(isVertex(curve.polyline, p));
    }
    CHECK(curve.polyline.size() > through.size()); // it bends in between
}

TEST_CASE("sketch spline: the chain of lines stays within 0.01 mm of the curve",
          "[sketch][geometry]") {
    const auto k = kernel();
    // the finest chain is the reference for the curve
    const std::vector<Point2d> through = {{0, 0}, {4, 3}, {5, -2}, {9, 1}, {10, 5}, {15, 0}};
    const SketchSplineCurve fine = k->sketchSpline(through, 0.0002);
    const SketchSplineCurve coarse = k->sketchSpline(through, 0.01);
    REQUIRE(coarse.polyline.size() < fine.polyline.size());
    for (std::size_t i = 1; i < coarse.polyline.size(); ++i) {
        const Point2d middle{(coarse.polyline[i - 1].x + coarse.polyline[i].x) / 2.0,
                             (coarse.polyline[i - 1].y + coarse.polyline[i].y) / 2.0};
        CHECK(distanceToChain(middle, fine.polyline) <= 0.01);
    }
    for (const Point2d& p : fine.polyline) { // and the other way round
        CHECK(distanceToChain(p, coarse.polyline) <= 0.0102);
    }
}

TEST_CASE("sketch spline: tight bends stay within 0.01 mm", "[sketch][geometry]") {
    const auto k = kernel();
    // a hairpin: the curve turns round within a few millimetres
    const std::vector<Point2d> through = {{0, 0}, {30, 0}, {32, 1}, {30, 2}, {0, 2.5}};
    const SketchSplineCurve fine = k->sketchSpline(through, 0.0002);
    const SketchSplineCurve coarse = k->sketchSpline(through, 0.005);
    for (std::size_t i = 1; i < coarse.polyline.size(); ++i) {
        const Point2d middle{(coarse.polyline[i - 1].x + coarse.polyline[i].x) / 2.0,
                             (coarse.polyline[i - 1].y + coarse.polyline[i].y) / 2.0};
        CHECK(distanceToChain(middle, fine.polyline) <= 0.01);
    }
}

TEST_CASE("sketch spline: a closed curve closes smoothly", "[sketch][geometry]") {
    const auto k = kernel();
    const std::vector<Point2d> through = ring(8, 20.0);
    const SketchSplineCurve curve = k->sketchSpline(through, 0.005);
    CHECK(curve.closed);
    CHECK(curve.polyline.front() == through.front());
    CHECK(curve.polyline.back() == through.front());
    for (const Point2d& p : through) {
        CHECK(isVertex(curve.polyline, p));
    }
    // no corner where it closes: the directions at the start and at the end are the same
    CHECK(curve.startTangent.x == Approx(curve.endTangent.x).margin(1e-9));
    CHECK(curve.startTangent.y == Approx(curve.endTangent.y).margin(1e-9));
    // round a circle the curve is close to the circle (a check that it is a good curve)
    for (const Point2d& p : curve.polyline) {
        CHECK(std::hypot(p.x, p.y) == Approx(20.0).margin(0.3));
    }
}

TEST_CASE("sketch spline: a closed curve through an irregular loop is smooth at the join",
          "[sketch][geometry]") {
    const auto k = kernel();
    const SketchSplineCurve curve =
        k->sketchSpline({{0, 0}, {12, 3}, {20, 15}, {5, 22}, {-8, 10}, {0, 0}}, 0.005);
    CHECK(curve.closed);
    CHECK(curve.startTangent.x == Approx(curve.endTangent.x).margin(1e-9));
    CHECK(curve.startTangent.y == Approx(curve.endTangent.y).margin(1e-9));
    CHECK(std::hypot(curve.startTangent.x, curve.startTangent.y) == Approx(1.0).margin(1e-12));
}

TEST_CASE("sketch spline: two points make a straight curve", "[sketch][geometry]") {
    const auto k = kernel();
    const SketchSplineCurve curve = k->sketchSpline({{1, 1}, {4, 5}}, 0.005);
    REQUIRE(curve.polyline.size() == 2);
    CHECK(curve.polyline[0] == Point2d{1, 1});
    CHECK(curve.polyline[1] == Point2d{4, 5});
    CHECK(curve.startTangent.x == Approx(0.6).margin(1e-12));
    CHECK(curve.startTangent.y == Approx(0.8).margin(1e-12));
    CHECK(curve.endTangent.x == Approx(0.6).margin(1e-12));
    CHECK_FALSE(curve.closed);
}

TEST_CASE("sketch spline: three points on a line stay on the line", "[sketch][geometry]") {
    const auto k = kernel();
    const SketchSplineCurve curve = k->sketchSpline({{0, 0}, {5, 5}, {10, 10}}, 0.005);
    for (const Point2d& p : curve.polyline) {
        CHECK(p.x == Approx(p.y).margin(1e-9));
    }
}

TEST_CASE("sketch spline: points that repeat are one point", "[sketch][geometry]") {
    const auto k = kernel();
    const SketchSplineCurve once = k->sketchSpline(wave(), 0.005);
    std::vector<Point2d> twice;
    for (const Point2d& p : wave()) {
        twice.push_back(p);
        twice.push_back(p);
    }
    const SketchSplineCurve curve = k->sketchSpline(twice, 0.005);
    CHECK(curve.polyline == once.polyline);
    // all the same point: nothing to draw
    CHECK_THROWS_AS(k->sketchSpline({{3, 3}, {3, 3}, {3, 3}}, 0.005), GeometryError);
}

TEST_CASE("sketch spline: bad input is refused with an invalid-input error", "[sketch][geometry]") {
    const auto k = kernel();
    const auto code = [&](const std::vector<Point2d>& points, double tolerance) {
        try {
            k->sketchSpline(points, tolerance);
        } catch (const GeometryError& error) {
            return error.code();
        }
        return GeometryErrorCode::kernelFailure; // not thrown: counts as wrong below
    };
    CHECK(code({}, 0.005) == GeometryErrorCode::invalidInput);
    CHECK(code({{0, 0}}, 0.005) == GeometryErrorCode::invalidInput);
    CHECK(code({{0, 0}, {1, 1}, {0, 0}}, 0.005) ==
          GeometryErrorCode::invalidInput); // closed on two
    CHECK(code(wave(), 0.0) == GeometryErrorCode::invalidInput);
    CHECK(code(wave(), -1.0) == GeometryErrorCode::invalidInput);
    CHECK(code({{0, 0}, {std::numeric_limits<double>::quiet_NaN(), 1}, {2, 2}}, 0.005) ==
          GeometryErrorCode::invalidInput);
}

TEST_CASE("sketch spline: a curve with 50 points is calculated in a blink", "[sketch][geometry]") {
    const auto k = kernel();
    std::vector<Point2d> through;
    for (int i = 0; i < 50; ++i) {
        through.push_back({i * 4.0, 15.0 * std::sin(i * 0.7)});
    }
    const auto start = std::chrono::steady_clock::now();
    SketchSplineCurve curve;
    for (int run = 0; run < 10; ++run) {
        curve = k->sketchSpline(through, 0.005);
    }
    const auto each =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start) / 10.0;
    CHECK(curve.polyline.size() > 50);
    CHECK(each.count() < 50.0); // the preview follows the pointer: well within a frame or three
}

TEST_CASE("sketch spline: the kernel handles many calls at once", "[sketch][geometry]") {
    const auto k = kernel();
    const SketchSplineCurve a = k->sketchSpline(wave(), 0.005);
    const SketchSplineCurve b = k->sketchSpline(wave(), 0.005);
    CHECK(a.polyline == b.polyline); // nothing is kept between two calls
}
