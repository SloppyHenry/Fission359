// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Snapping (AP-1.6c): the points a body can catch on, and the rule that moves a body onto one.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "fission/geo/mesh.h"
#include "fission/geo/occt/solids.h"
#include "fission/interaction/drag.h"
#include "fission/interaction/snap.h"

using Catch::Approx;
using namespace fission::interaction;
namespace occt = fission::geo::occt;

namespace {

std::size_t countOf(const std::vector<SnapPoint>& points, SnapKind kind) {
    return static_cast<std::size_t>(std::count_if(
        points.begin(), points.end(), [kind](const SnapPoint& p) { return p.kind == kind; }));
}

bool has(const std::vector<SnapPoint>& points, SnapKind kind, const Vec3& at) {
    return std::any_of(points.begin(), points.end(), [&](const SnapPoint& p) {
        return p.kind == kind && std::abs(p.position[0] - at[0]) < 1e-6 &&
               std::abs(p.position[1] - at[1]) < 1e-6 && std::abs(p.position[2] - at[2]) < 1e-6;
    });
}

const Vec3 kReach{2.0, 2.0, 2.0};
const std::array<bool, 3> kAll{true, true, true};
const std::array<bool, 3> kAcross{true, true, false};

} // namespace

TEST_CASE("snap points of a box: 8 corners, 12 edge middles, 6 face middles", "[snap]") {
    const auto points = snapPointsOfBox({0, 0, 0}, {10, 20, 30});
    CHECK(points.size() == 26);
    CHECK(countOf(points, SnapKind::corner) == 8);
    CHECK(countOf(points, SnapKind::edgeMiddle) == 12);
    CHECK(countOf(points, SnapKind::faceMiddle) == 6);
    CHECK(has(points, SnapKind::edgeMiddle, {5, 0, 0}));
    CHECK(has(points, SnapKind::faceMiddle, {5, 10, 30}));
}

TEST_CASE("snap points of the bed: corners, edge middles and the middle", "[snap]") {
    const auto points = snapPointsOfBed(256.0, 200.0);
    CHECK(countOf(points, SnapKind::corner) == 4);
    CHECK(countOf(points, SnapKind::edgeMiddle) == 4);
    CHECK(countOf(points, SnapKind::faceMiddle) == 1);
    CHECK(has(points, SnapKind::corner, {-128, 100, 0}));
    CHECK(has(points, SnapKind::edgeMiddle, {0, -100, 0}));
}

TEST_CASE("snap points of a mesh: a box has corners, edge middles and face middles", "[snap]") {
    const auto mesh = occt::tessellate(occt::makeBox(20.0, 30.0, 40.0, {-10.0, -15.0, 0.0}));
    const auto points = snapPointsOf(mesh);
    CHECK(countOf(points, SnapKind::corner) == 8);
    CHECK(countOf(points, SnapKind::edgeMiddle) == 12);
    CHECK(countOf(points, SnapKind::faceMiddle) == 6);
    CHECK(countOf(points, SnapKind::circleCentre) == 0);
    CHECK(has(points, SnapKind::corner, {-10, -15, 0}));
    CHECK(has(points, SnapKind::edgeMiddle, {0, -15, 40}));
    CHECK(has(points, SnapKind::faceMiddle, {0, 0, 40}));
}

TEST_CASE("snap points of a mesh: a cylinder has the centres of its two circles", "[snap]") {
    const auto mesh = occt::tessellate(occt::makeCylinder(10.0, 25.0, {5.0, 7.0, 0.0}));
    const auto points = snapPointsOf(mesh);
    CHECK(has(points, SnapKind::circleCentre, {5, 7, 0}));
    CHECK(has(points, SnapKind::circleCentre, {5, 7, 25}));
    CHECK(countOf(points, SnapKind::circleCentre) == 2);
    // The middles of the flat ends are the same places: the round side has none.
    CHECK(has(points, SnapKind::faceMiddle, {5, 7, 25}));
    CHECK(countOf(points, SnapKind::faceMiddle) == 2);
}

TEST_CASE("snap: a point of the body catches on a point of the others", "[snap]") {
    const auto movers = snapPointsOfBox({0, 0, 0}, {10, 10, 10});
    const auto targets = snapPointsOfBox({30, 0, 0}, {50, 10, 10});
    // The body is pulled to x = 19.3 (its right side) : 0.7 short of the left side of the other.
    const auto result = snapOffset(movers, targets, {19.3, 0.5, 0.0}, kAcross, kReach);
    CHECK(result.caught());
    CHECK(result.offset[0] == Approx(20.0));
    CHECK(result.offset[1] == Approx(0.0)); // the sides line up in y as well
    REQUIRE(result.axes[0]);
    REQUIRE(result.axes[1]);
    CHECK_FALSE(result.axes[2]);
    // One point on one point: a point catch, the same pair on both axes.
    CHECK(result.axes[0]->onPoint());
    CHECK(result.axes[0]->target == result.axes[1]->target);
}

TEST_CASE("snap: out of reach nothing happens", "[snap]") {
    const auto movers = snapPointsOfBox({0, 0, 0}, {10, 10, 10});
    const auto targets = snapPointsOfBox({30, 0, 0}, {50, 10, 10});
    const auto result = snapOffset(movers, targets, {15.0, 2.5, 0.0}, kAcross, kReach);
    CHECK_FALSE(result.caught());
    CHECK(result.offset[0] == Approx(15.0));
    CHECK(result.offset[1] == Approx(2.5));
}

TEST_CASE("snap: along one axis the body lines up with a corner beside it", "[snap]") {
    // Sliding along x with y a little off: only x is free, the others stay as they are.
    const auto movers = snapPointsOfBox({0, 0, 0}, {10, 10, 10});
    const auto targets = snapPointsOfBox({40, 40, 0}, {60, 60, 10});
    const auto result = snapOffset(movers, targets, {29.4, 3.0, 0.0}, {true, false, false}, kReach);
    CHECK(result.offset[0] == Approx(30.0)); // right side of the body on the left side of the other
    CHECK(result.offset[1] == Approx(3.0));
    REQUIRE(result.axes[0]);
    // The two are lined up, not on each other: a line joins them.
    CHECK_FALSE(result.axes[0]->onPoint());
    CHECK(result.axes[0]->mover.position[0] == Approx(result.axes[0]->target.position[0]));
}

TEST_CASE("snap: of several equal line-ups the nearest pair is shown", "[snap]") {
    const auto movers = snapPointsOfBox({0, 0, 0}, {10, 10, 10});
    // Two bodies the same distance away in x, one close in y, one far.
    auto targets = snapPointsOfBox({30, 100, 0}, {40, 110, 10});
    const auto near = snapPointsOfBox({30, 0, 0}, {40, 10, 10});
    targets.insert(targets.end(), near.begin(), near.end());
    const auto result = snapOffset(movers, targets, {20.0, 0.0, 0.0}, {true, false, false}, kReach);
    REQUIRE(result.axes[0]);
    CHECK(result.axes[0]->target.position[1] < 50.0);
}

TEST_CASE("snap: the reach is a distance per axis", "[snap]") {
    const auto movers = snapPointsOfBox({0, 0, 0}, {10, 10, 10});
    const auto targets = snapPointsOfBox({30, 0, 0}, {50, 10, 10});
    // Wide reach in x, none in y: x catches, y stays.
    const auto result = snapOffset(movers, targets, {17.0, 0.7, 0.0}, kAcross, {4.0, 0.0, 0.0});
    CHECK(result.offset[0] == Approx(20.0));
    CHECK(result.offset[1] == Approx(0.7));
    CHECK_FALSE(result.axes[1]);
}

TEST_CASE("snap: the bottom of a body catches on the bed", "[snap]") {
    const auto movers = snapPointsOfBox({0, 0, 5}, {10, 10, 15});
    const auto targets = snapPointsOfBed(256.0, 256.0);
    const auto result = snapOffset(movers, targets, {0.0, 0.0, -4.2}, {false, false, true}, kReach);
    CHECK(result.offset[2] == Approx(-5.0));
}

TEST_CASE("snap: the body is not caught on itself and a body without targets stays", "[snap]") {
    const auto movers = snapPointsOfBox({0, 0, 0}, {10, 10, 10});
    const auto result = snapOffset(movers, {}, {1.0, 2.0, 3.0}, kAll, kReach);
    CHECK_FALSE(result.caught());
    CHECK(result.offset[0] == Approx(1.0));
}

TEST_CASE("snap: floating point noise of the mesh is cleaned away", "[snap]") {
    fission::geo::TriangleMesh mesh;
    mesh.corners.push_back({1, {12.3F, -0.1F, 7.7F}});
    const auto points = snapPointsOf(mesh);
    REQUIRE(points.size() == 1);
    // 12.3 as a single precision number is 12.30000019...: it reads 12.3 again.
    CHECK(points[0].position[0] == 12.3);
    CHECK(points[0].position[1] == -0.1);
    CHECK(points[0].position[2] == 7.7);
}

TEST_CASE("snap grid on the bed: the lines are never closer than 8 pixels", "[snap]") {
    for (const double zoom : {0.2, 0.5, 1.0, 3.0, 6.0, 12.0, 25.0, 60.0}) {
        // Every grid the user can choose, and the one of the zoom.
        for (const double snap : {fission::interaction::snapStep(zoom), 10.0, 1.0, 0.1, 0.01}) {
            const double step = snapGridDisplayStep(snap, zoom);
            CHECK(step * zoom >= 8.0 - 1e-9);
            // The lines lie on the grid a drag snaps to: the display step is a multiple of it.
            const double times = step / snap;
            CHECK(std::abs(times - std::round(times)) < 1e-6);
        }
    }
    CHECK(snapGridDisplayStep(0.5, 10.0) == Approx(1.0));
    CHECK(snapGridDisplayStep(10.0, 10.0) == Approx(10.0));
}
