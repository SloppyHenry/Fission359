// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Distance measures (AP-1.6d): the free space between a body and its neighbours or the edge of
// the bed, and the move that makes it a wanted number.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

#include "fission/interaction/distances.h"

using Catch::Approx;
using namespace fission::interaction;

namespace {

using Others = std::vector<std::pair<std::string, Box3>>;

// The measure of one side, by value (so that it can be taken from a temporary list).
std::optional<Gap> find(const std::vector<Gap>& gaps, Side side) {
    const auto it =
        std::find_if(gaps.begin(), gaps.end(), [side](const Gap& gap) { return gap.side == side; });
    return it == gaps.end() ? std::nullopt : std::optional<Gap>(*it);
}

// A 20 mm cube standing on the bed with its middle at (x, y).
Box3 cube(double x, double y, double size = 20.0) {
    return {{x - size / 2.0, y - size / 2.0, 0.0}, {x + size / 2.0, y + size / 2.0, size}};
}

} // namespace

TEST_CASE("distances: a body on the left with a gap of 7 mm and overlap in Y", "[distance]") {
    const Box3 me = cube(0, 0);
    const Others others{{"left", cube(-37, 5)}}; // its right side at -27, mine at -10: 17 mm
    const auto gaps = nearestGaps(me, others, 256, 256);
    const auto gap = find(gaps, Side::left);
    REQUIRE(gap.has_value());
    CHECK_FALSE(gap->toBed);
    CHECK(gap->otherBody == "left");
    CHECK(gap->distance == Approx(17.0));

    const Others near{{"left", cube(-27, 0)}}; // right side at -17: 7 mm
    const auto seven = find(nearestGaps(me, near, 256, 256), Side::left);
    REQUIRE(seven.has_value());
    CHECK(seven->distance == Approx(7.0));
    // The band runs from my side to the side of the other, in the middle of the overlap.
    CHECK(seven->from[0] == Approx(-10.0));
    CHECK(seven->to[0] == Approx(-17.0));
    CHECK(seven->from[1] == Approx(0.0));
    CHECK(seven->from[2] == Approx(10.0));
}

TEST_CASE("distances: of two bodies on one side the nearer counts", "[distance]") {
    const Others others{{"far", cube(-49, 0)}, {"near", cube(-23, 0)}}; // 9 mm and 3 mm
    const auto gap = find(nearestGaps(cube(0, 0), others, 256, 256), Side::left);
    REQUIRE(gap.has_value());
    CHECK(gap->otherBody == "near");
    CHECK(gap->distance == Approx(3.0));
}

TEST_CASE("distances: a body with no overlap in Y is not in the way", "[distance]") {
    const Others others{{"aside", cube(-30, 40)}}; // left of me, but 20 mm further back
    const auto gaps = nearestGaps(cube(0, 0), others, 256, 256);
    const auto left = find(gaps, Side::left);
    REQUIRE(left.has_value());
    CHECK(left->toBed);
    CHECK(left->distance == Approx(128.0 - 10.0));
}

TEST_CASE("distances: with no neighbour all four measures go to the edge of the bed",
          "[distance]") {
    const auto gaps = nearestGaps(cube(10, -20), {}, 256, 200);
    REQUIRE(gaps.size() == 4);
    CHECK(find(gaps, Side::left)->distance == Approx(128.0));  // -128 .. 0
    CHECK(find(gaps, Side::right)->distance == Approx(108.0)); // 20 .. 128
    CHECK(find(gaps, Side::front)->distance == Approx(70.0));  // -100 .. -30
    CHECK(find(gaps, Side::back)->distance == Approx(110.0));  // -10 .. 100
    for (const Gap& gap : gaps) {
        CHECK(gap.toBed);
        CHECK(gap.otherBody.empty());
        CHECK(gap.from[2] == Approx(kBedBandHeight));
    }
    // The band to the left edge runs from my side to x = -128, at the middle of my depth.
    const auto left = find(gaps, Side::left);
    REQUIRE(left.has_value());
    CHECK(left->from[0] == Approx(0.0));
    CHECK(left->to[0] == Approx(-128.0));
    CHECK(left->from[1] == Approx(-20.0));
}

TEST_CASE("distances: bodies that overlap are no gap", "[distance]") {
    const Others others{{"inside", cube(-5, 0)}, {"above", cube(0, 0)}};
    const auto gaps = nearestGaps(cube(0, 0), others, 256, 256);
    for (const Gap& gap : gaps) {
        CHECK(gap.toBed); // nothing counts as a neighbour
    }
}

TEST_CASE("distances: touching bodies have a gap of 0", "[distance]") {
    const Others others{{"touching", cube(-20, 0)}};
    const auto gap = find(nearestGaps(cube(0, 0), others, 256, 256), Side::left);
    REQUIRE(gap.has_value());
    CHECK_FALSE(gap->toBed);
    CHECK(gap->distance == Approx(0.0).margin(1e-9));
}

TEST_CASE("distances: over the edge of the bed there is no measure in that direction",
          "[distance]") {
    // 20 mm cube whose left side is 8 mm beyond the left edge of a 100 mm bed.
    const auto gaps = nearestGaps(cube(-48, 0), {}, 100, 100);
    CHECK_FALSE(find(gaps, Side::left).has_value());
    CHECK(find(gaps, Side::right).has_value());
}

TEST_CASE("distances: the noise of a mesh is taken off", "[distance]") {
    const Box3 me{{0.0, 0.0, 0.0}, {10.0, 10.0, 10.0}};
    const Others others{{"n", {{-12.30000019, 0.0, 0.0}, {-5.00000024, 10.0, 10.0}}}};
    const auto gap = find(nearestGaps(me, others, 256, 256), Side::left);
    REQUIRE(gap.has_value());
    CHECK(gap->distance == Approx(5.0).margin(1e-12));
    CHECK(gap->distance == 5.0);
}

TEST_CASE("distances: setting a distance moves the body so that it measures that", "[distance]") {
    struct Case {
        Side side;
        Box3 other;
    };
    // The other body 7 mm from mine on each side.
    const Case cases[] = {{Side::left, cube(-27, 0)},
                          {Side::right, cube(27, 0)},
                          {Side::front, cube(0, -27)},
                          {Side::back, cube(0, 27)}};
    for (const Case& test : cases) {
        for (const double wanted : {5.0, 12.0, 0.0}) {
            const Box3 me = cube(0, 0);
            const Others others{{"other", test.other}};
            const auto gap = find(nearestGaps(me, others, 256, 256), test.side);
            REQUIRE(gap.has_value());
            REQUIRE(gap->distance == Approx(7.0));
            const auto move = offsetForDistance(*gap, wanted);
            REQUIRE(move.has_value());
            // Only along one axis, and only in that axis.
            CHECK(((*move)[0] == 0.0 || (*move)[1] == 0.0));
            CHECK((*move)[2] == 0.0);

            Box3 moved = me;
            for (std::size_t i = 0; i < 3; ++i) {
                moved.low[i] += (*move)[i];
                moved.high[i] += (*move)[i];
            }
            const auto after = find(nearestGaps(moved, others, 256, 256), test.side);
            REQUIRE(after.has_value());
            CHECK(after->distance == Approx(wanted).margin(1e-9));
        }
    }
}

TEST_CASE("distances: a distance to the bed edge can be set as well", "[distance]") {
    const Box3 me = cube(0, 0);
    const auto gap = find(nearestGaps(me, {}, 256, 256), Side::left);
    REQUIRE(gap.has_value());
    const auto move = offsetForDistance(*gap, 5.0);
    REQUIRE(move.has_value());
    // 118 mm from the edge -> 5 mm: 113 mm to the left.
    CHECK((*move)[0] == Approx(-113.0));
}

TEST_CASE("distances: a negative number or no number moves nothing", "[distance]") {
    const auto gaps = nearestGaps(cube(0, 0), {}, 256, 256);
    const auto gap = find(gaps, Side::right);
    REQUIRE(gap.has_value());
    CHECK_FALSE(offsetForDistance(*gap, -1.0).has_value());
    CHECK_FALSE(offsetForDistance(*gap, std::nan("")).has_value());
    CHECK(offsetForDistance(*gap, 0.0).has_value());
}
