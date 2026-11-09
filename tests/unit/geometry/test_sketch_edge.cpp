// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Edges of a body projected onto a sketch plane (AP-2.1f, decision 46): lines, circles and arcs
// exactly (1e-9 mm), everything else reported and never approximated.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

constexpr double kExact = 1e-9;

std::unique_ptr<GeometryKernel> kernel() {
    return occt::makeKernel();
}

// The plane z = height, seen from above: u = X, v = Y.
SketchFrame above(double height = 0.0) {
    return {{0.0, 0.0, height}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
}

// The number of the first edge of the kind whose middle is near `where`.
std::uint32_t edgeAt(const GeometryKernel& k, const Solid& solid, SignatureShape shape,
                     Point3d where) {
    const BodySignatures sigs = k.signatures(solid);
    for (const ElementSignature& edge : sigs.edges) {
        if (edge.shape == shape && std::abs(edge.centroid.x - where.x) < 1e-6 &&
            std::abs(edge.centroid.y - where.y) < 1e-6 &&
            std::abs(edge.centroid.z - where.z) < 1e-6) {
            return edge.id;
        }
    }
    FAIL("no such edge");
    return 0;
}

bool near(const Point2d& a, double x, double y) {
    return std::abs(a.x - x) < kExact && std::abs(a.y - y) < kExact;
}

// A line has two ends, in either order.
bool isLine(const SketchEdgeShape& shape, Point2d a, Point2d b) {
    return shape.status == SketchEdgeStatus::ok && shape.kind == SketchCurveKind::line &&
           shape.points.size() == 2 &&
           ((near(shape.points[0], a.x, a.y) && near(shape.points[1], b.x, b.y)) ||
            (near(shape.points[0], b.x, b.y) && near(shape.points[1], a.x, a.y)));
}

} // namespace

TEST_CASE("projecting an edge: a straight edge parallel to the plane is the same line",
          "[sketch][edge][golden]") {
    const auto k = kernel();
    const NamedSolid box = k->makeBox(40.0, 20.0, 10.0, {-20.0, -10.0, 0.0}, 0.0);
    const std::uint32_t edge = edgeAt(*k, box.solid, SignatureShape::line, {0.0, -10.0, 10.0});
    const SketchEdgeShape shape = k->projectEdgeToSketch(box.solid, edge, above(10.0));
    CHECK(isLine(shape, {-20.0, -10.0}, {20.0, -10.0}));
    // The plane may lie anywhere along its normal: the projection is perpendicular.
    CHECK(
        isLine(k->projectEdgeToSketch(box.solid, edge, above(0.0)), {-20.0, -10.0}, {20.0, -10.0}));
    // A moved origin and turned axes: plane coordinates, not world coordinates.
    const SketchFrame turned{{5.0, 2.0, 10.0}, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}};
    // u = world Y, v = -world X: (x, y) = (±20, -10) -> u = y - 2 = -12, v = -(x - 5)
    CHECK(isLine(k->projectEdgeToSketch(box.solid, edge, turned), {-12.0, -(-20.0 - 5.0)},
                 {-12.0, -(20.0 - 5.0)}));
}

TEST_CASE("projecting an edge: a slanted edge is shortened exactly", "[sketch][edge][golden]") {
    const auto k = kernel();
    NamedSolid box = k->makeBox(40.0, 20.0, 10.0, {-20.0, -10.0, 0.0}, 0.0);
    box = k->rotate(box, {0.0, 0.0, 0.0}, {30.0, 0.0, 0.0}); // about X: the Y edges slant
    const double c = std::cos(30.0 * std::numbers::pi / 180.0);
    const double s = std::sin(30.0 * std::numbers::pi / 180.0);
    // The bottom edge on the left, from (-20, -10, 0) to (-20, 10, 0), turned about X.
    const BodySignatures sigs = k->signatures(box.solid);
    std::uint32_t found = 0;
    for (const ElementSignature& edge : sigs.edges) {
        if (edge.shape == SignatureShape::line && std::abs(edge.centroid.x + 20.0) < 1e-6 &&
            std::abs(edge.centroid.y) < 1e-6 && std::abs(edge.centroid.z) < 1e-6) {
            found = edge.id; // middle (-20, 0, 0): the left bottom edge
        }
    }
    REQUIRE(found != 0);
    const SketchEdgeShape shape = k->projectEdgeToSketch(box.solid, found, above());
    // ends: (-20, ∓10 cos30, ∓10 sin30) -> on the plane z = 0 the shortened line
    CHECK(isLine(shape, {-20.0, -10.0 * c}, {-20.0, 10.0 * c}));
    (void)s;
}

TEST_CASE("projecting an edge: an edge along the normal would be a point", "[sketch][edge]") {
    const auto k = kernel();
    const NamedSolid box = k->makeBox(40.0, 20.0, 10.0, {-20.0, -10.0, 0.0}, 0.0);
    const std::uint32_t edge = edgeAt(*k, box.solid, SignatureShape::line, {20.0, 10.0, 5.0});
    const SketchEdgeShape shape = k->projectEdgeToSketch(box.solid, edge, above());
    CHECK(shape.status == SketchEdgeStatus::perpendicular);
    CHECK(shape.points.empty());
}

TEST_CASE("projecting an edge: a circle with its axis along the normal stays a circle",
          "[sketch][edge][golden]") {
    const auto k = kernel();
    const NamedSolid cylinder = k->makeCylinder(7.0, 10.0, {3.0, 4.0, 0.0});
    const std::uint32_t top = edgeAt(*k, cylinder.solid, SignatureShape::circle, {3.0, 4.0, 10.0});
    const SketchEdgeShape shape = k->projectEdgeToSketch(cylinder.solid, top, above(10.0));
    REQUIRE(shape.status == SketchEdgeStatus::ok);
    CHECK(shape.kind == SketchCurveKind::circle);
    REQUIRE(shape.points.size() == 2);
    CHECK(near(shape.points[0], 3.0, 4.0));
    CHECK(std::hypot(shape.points[1].x - 3.0, shape.points[1].y - 4.0) ==
          Approx(7.0).margin(kExact));
    // Seen from below (the plane's normal points down) the circle is the same circle.
    const SketchFrame below{{0.0, 0.0, 10.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}};
    const SketchEdgeShape flipped = k->projectEdgeToSketch(cylinder.solid, top, below);
    REQUIRE(flipped.status == SketchEdgeStatus::ok);
    CHECK(near(flipped.points[0], 3.0, -4.0));
    CHECK(std::hypot(flipped.points[1].x - 3.0, flipped.points[1].y + 4.0) ==
          Approx(7.0).margin(kExact));
}

TEST_CASE("projecting an edge: an arc runs counter-clockwise on the plane, from either side",
          "[sketch][edge][golden]") {
    const auto k = kernel();
    // A box with the vertical edges rounded with radius 5: four quarter arcs (axes along Z).
    const NamedSolid box = k->makeBox(40.0, 20.0, 10.0, {-20.0, -10.0, 0.0}, 5.0);
    const BodySignatures sigs = k->signatures(box.solid);
    std::uint32_t arcEdge = 0;
    for (const ElementSignature& edge : sigs.edges) {
        // the top arc of the corner at the right front: middle of the arc is not on the arc,
        // so look for a circle edge at height 10 with x > 0, y < 0
        if (edge.shape == SignatureShape::circle && std::abs(edge.centroid.z - 10.0) < 1e-6 &&
            edge.centroid.x > 0.0 && edge.centroid.y < 0.0) {
            arcEdge = edge.id;
        }
    }
    REQUIRE(arcEdge != 0);
    for (const bool up : {true, false}) {
        const SketchFrame frame =
            up ? above(10.0) : SketchFrame{{0.0, 0.0, 10.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}};
        const SketchEdgeShape shape = k->projectEdgeToSketch(box.solid, arcEdge, frame);
        REQUIRE(shape.status == SketchEdgeStatus::ok);
        REQUIRE(shape.kind == SketchCurveKind::arc);
        REQUIRE(shape.points.size() == 3);
        const double sign = up ? 1.0 : -1.0; // v = ±Y
        CHECK(near(shape.points[0], 15.0, sign * -5.0));
        const Point2d from{shape.points[1].x - shape.points[0].x,
                           shape.points[1].y - shape.points[0].y};
        const Point2d to{shape.points[2].x - shape.points[0].x,
                         shape.points[2].y - shape.points[0].y};
        CHECK(std::hypot(from.x, from.y) == Approx(5.0).margin(kExact));
        CHECK(std::hypot(to.x, to.y) == Approx(5.0).margin(kExact));
        // a quarter, counter-clockwise on the plane
        CHECK(from.x * to.y - from.y * to.x == Approx(25.0).margin(1e-7));
        CHECK(from.x * to.x + from.y * to.y == Approx(0.0).margin(1e-7));
        // the end points are the arc's real ends: (20, -5) and (15, -10) in the world
        const auto world = [&](const Point2d& p) { return Point2d{p.x, sign * p.y}; };
        const Point2d a = world(shape.points[1]);
        const Point2d b = world(shape.points[2]);
        const bool ends = (near(a, 20.0, -5.0) && near(b, 15.0, -10.0)) ||
                          (near(b, 20.0, -5.0) && near(a, 15.0, -10.0));
        CHECK(ends);
    }
}

TEST_CASE("projecting an edge: a tilted circle would be an ellipse and is not taken",
          "[sketch][edge]") {
    const auto k = kernel();
    NamedSolid cylinder = k->makeCylinder(7.0, 10.0, {0.0, 0.0, 0.0});
    cylinder = k->rotate(cylinder, {0.0, 0.0, 0.0}, {30.0, 0.0, 0.0});
    const BodySignatures sigs = k->signatures(cylinder.solid);
    int circles = 0;
    for (const ElementSignature& edge : sigs.edges) {
        if (edge.shape != SignatureShape::circle) {
            continue; // the seam of the side
        }
        ++circles;
        const SketchEdgeShape shape = k->projectEdgeToSketch(cylinder.solid, edge.id, above());
        CHECK(shape.status == SketchEdgeStatus::notExact);
        CHECK(shape.points.empty());
    }
    CHECK(circles == 2);
    // A circle seen edge-on is a line segment, which is not the circle: not taken either.
    const NamedSolid upright = k->makeCylinder(7.0, 10.0, {0.0, 0.0, 0.0});
    const SketchFrame front{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}};
    const std::uint32_t top = edgeAt(*k, upright.solid, SignatureShape::circle, {0.0, 0.0, 10.0});
    CHECK(k->projectEdgeToSketch(upright.solid, top, front).status == SketchEdgeStatus::notExact);
}

TEST_CASE("projecting an edge: an edge that is not there or a plane that is no plane is refused",
          "[sketch][edge]") {
    const auto k = kernel();
    const NamedSolid box = k->makeBox(10.0, 10.0, 10.0, {0.0, 0.0, 0.0}, 0.0);
    CHECK_THROWS_AS(k->projectEdgeToSketch(box.solid, 0, above()), GeometryError);
    CHECK_THROWS_AS(k->projectEdgeToSketch(box.solid, 99, above()), GeometryError);
    const SketchFrame crooked{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    CHECK_THROWS_AS(k->projectEdgeToSketch(box.solid, 1, crooked), GeometryError);
    CHECK_THROWS_AS(k->projectEdgeToSketch(Solid{}, 1, above()), GeometryError);
}
