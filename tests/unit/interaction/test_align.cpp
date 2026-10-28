// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Aligning bodies (AP-1.6e): the moves that put them flush or in the middle.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fission/interaction/align.h"

using Catch::Approx;
using namespace fission::interaction;

namespace {

// Three boxes of different size and place: x 0..10, 20..50, 5..15.
const std::vector<Box3> kBoxes{
    {{0, 0, 0}, {10, 10, 10}}, {{20, 5, 0}, {50, 25, 40}}, {{5, -10, 3}, {15, 0, 8}}};

} // namespace

TEST_CASE("align: left, middle and right along X use the box round all of them", "[align]") {
    // The box round them: x from 0 to 50.
    const auto left = alignOffsets(kBoxes, 0, AlignTo::min);
    CHECK(left[0][0] == Approx(0.0));
    CHECK(left[1][0] == Approx(-20.0));
    CHECK(left[2][0] == Approx(-5.0));

    const auto right = alignOffsets(kBoxes, 0, AlignTo::max);
    CHECK(right[0][0] == Approx(40.0));
    CHECK(right[1][0] == Approx(0.0));
    CHECK(right[2][0] == Approx(35.0));

    const auto middle = alignOffsets(kBoxes, 0, AlignTo::centre);
    CHECK(middle[0][0] == Approx(20.0));  // 5 -> 25
    CHECK(middle[1][0] == Approx(-10.0)); // 35 -> 25
    CHECK(middle[2][0] == Approx(15.0));  // 10 -> 25
}

TEST_CASE("align: only the chosen axis moves", "[align]") {
    for (const int axis : {0, 1, 2}) {
        const auto offsets = alignOffsets(kBoxes, axis, AlignTo::centre);
        for (const Vec3& offset : offsets) {
            for (int other = 0; other < 3; ++other) {
                if (other != axis) {
                    CHECK(offset[static_cast<std::size_t>(other)] == 0.0);
                }
            }
        }
    }
}

TEST_CASE("align: after the moves every box has the same line", "[align]") {
    for (const int axis : {0, 1, 2}) {
        for (const AlignTo to : {AlignTo::min, AlignTo::centre, AlignTo::max}) {
            const auto offsets = alignOffsets(kBoxes, axis, to);
            const auto a = static_cast<std::size_t>(axis);
            double first = 0.0;
            for (std::size_t i = 0; i < kBoxes.size(); ++i) {
                const double low = kBoxes[i].low[a] + offsets[i][a];
                const double high = kBoxes[i].high[a] + offsets[i][a];
                const double line = to == AlignTo::min   ? low
                                    : to == AlignTo::max ? high
                                                         : (low + high) / 2.0;
                if (i == 0) {
                    first = line;
                }
                CHECK(line == Approx(first).margin(1e-9));
            }
        }
    }
}

TEST_CASE("align: one box, or no axis, moves nothing", "[align]") {
    const auto one = alignOffsets({kBoxes[0]}, 0, AlignTo::max);
    REQUIRE(one.size() == 1);
    CHECK(one[0] == Vec3{0, 0, 0});
    for (const Vec3& offset : alignOffsets(kBoxes, 7, AlignTo::min)) {
        CHECK(offset == Vec3{0, 0, 0});
    }
}
