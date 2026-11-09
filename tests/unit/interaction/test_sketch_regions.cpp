// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The regions of a sketch with their ids (AP-2.1g) as pure logic.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "fission/geo/occt/kernel.h"
#include "fission/interaction/sketch_regions.h"

using namespace fission::interaction;
using fission::core::SketchElement;
using fission::core::SketchElementKind;
using fission::core::SketchPoint;

namespace {

SketchElement line(std::string id, SketchPoint a, SketchPoint b, bool helper = false) {
    return {std::move(id), SketchElementKind::line, {a, b}, helper};
}
SketchElement circle(std::string id, SketchPoint middle, double radius) {
    return {
        std::move(id), SketchElementKind::circle, {middle, {middle.u + radius, middle.v}}, false};
}
std::vector<SketchElement> rectangle(double x, double y) {
    return {line("L1", {0, 0}, {x, 0}), line("L2", {x, 0}, {x, y}), line("L3", {x, y}, {0, y}),
            line("L4", {0, y}, {0, 0})};
}
std::vector<std::string> idsOf(const SketchRegionSet& set) {
    std::vector<std::string> out;
    for (const SketchRegionInfo& info : set.regions) {
        out.push_back(info.id);
    }
    return out;
}
std::vector<std::string> sorted(std::vector<std::string> ids) {
    std::ranges::sort(ids);
    return ids;
}

} // namespace

TEST_CASE("sketch regions ui: a rectangle is named after its lines", "[sketch][regions]") {
    const auto kernel = fission::geo::occt::makeKernel();
    const auto set = computeSketchRegions(*kernel, rectangle(40, 30));
    REQUIRE(set.regions.size() == 1);
    CHECK(set.regions[0].id == "L1+L2+L3+L4");
    CHECK(set.regions[0].elements.size() == 4);
}

TEST_CASE("sketch regions ui: moving an element keeps the ids, changing the shape does not",
          "[sketch][regions]") {
    const auto kernel = fission::geo::occt::makeKernel();
    auto elements = rectangle(40, 30);
    elements.push_back(circle("C1", {40, 15}, 5));
    const auto before = idsOf(computeSketchRegions(*kernel, elements));
    REQUIRE(before.size() == 3);
    // the three share lines or a circle: ids differ from each other
    CHECK(before[0] != before[1]);
    CHECK(before[1] != before[2]);

    elements[4].points = {{38, 12}, {43, 12}}; // the circle moves and gets smaller
    const auto moved = idsOf(computeSketchRegions(*kernel, elements));
    CHECK(sorted(moved) == sorted(before));

    elements.pop_back(); // the circle goes: one region, a new set of elements
    CHECK(idsOf(computeSketchRegions(*kernel, elements)) ==
          std::vector<std::string>{"L1+L2+L3+L4"});
}

TEST_CASE("sketch regions ui: two circles give a lens and two rests with different ids",
          "[sketch][regions]") {
    const auto kernel = fission::geo::occt::makeKernel();
    const auto set =
        computeSketchRegions(*kernel, {circle("C1", {0, 0}, 10), circle("C2", {10, 0}, 10)});
    REQUIRE(set.regions.size() == 3);
    const auto ids = idsOf(set);
    CHECK(ids[0] == "C1+C2/ll"); // the lens, smallest
    CHECK(ids[1] != ids[2]);
    CHECK((ids[1] == "C1+C2/lr" || ids[1] == "C1+C2/rl"));
    // the same after moving
    const auto again =
        idsOf(computeSketchRegions(*kernel, {circle("C1", {1, 2}, 10), circle("C2", {13, 2}, 10)}));
    CHECK(again[0] == ids[0]);
    CHECK((again[1] == ids[1] || again[1] == ids[2]));
}

TEST_CASE("sketch regions ui: helper lines and points make no regions", "[sketch][regions]") {
    const auto kernel = fission::geo::occt::makeKernel();
    auto elements = rectangle(40, 30);
    elements.push_back(line("L5", {20, -5}, {20, 35}, true));
    elements.push_back({"P1", SketchElementKind::point, {{5, 5}}, false});
    const auto set = computeSketchRegions(*kernel, elements);
    REQUIRE(set.regions.size() == 1);
    CHECK(set.regions[0].id == "L1+L2+L3+L4");
    // only helper lines: nothing
    CHECK(computeSketchRegions(*kernel, {line("L1", {0, 0}, {1, 0}, true)}).regions.empty());
}

TEST_CASE("sketch regions ui: the smallest region under a point is found", "[sketch][regions]") {
    const auto kernel = fission::geo::occt::makeKernel();
    auto elements = rectangle(40, 30);
    elements.push_back(circle("C1", {20, 15}, 5));
    const auto set = computeSketchRegions(*kernel, elements);
    REQUIRE(set.regions.size() == 2);
    const int disc = regionAt(set, {20, 15});
    REQUIRE(disc >= 0);
    CHECK(set.regions[static_cast<std::size_t>(disc)].elements == std::vector<std::string>{"C1"});
    const int ring = regionAt(set, {5, 5});
    REQUIRE(ring >= 0);
    CHECK(ring != disc);
    CHECK(regionAt(set, {100, 100}) == -1);
    CHECK_FALSE(regionContains(set.regions[static_cast<std::size_t>(ring)].shape, {20, 15}));
}
