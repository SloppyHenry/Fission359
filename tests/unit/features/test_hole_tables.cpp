// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <span>

#include "fission/features/hole_tables.h"

using Catch::Approx;
using namespace fission::features;

TEST_CASE("metric screw table entries are consistent", "[features][holes]") {
    const std::span<const MetricScrew> screws = metricScrews();
    REQUIRE(screws.size() == 9);

    for (const MetricScrew& screw : screws) {
        CHECK(screw.clearanceFine < screw.clearanceMedium);
        CHECK(screw.clearanceMedium < screw.clearanceCoarse);
        CHECK(screw.clearanceFine > screw.nominal);
        CHECK(screw.clearanceMedium > screw.nominal);
        CHECK(screw.clearanceCoarse > screw.nominal);
        CHECK(screw.tapDrill < screw.nominal);
        CHECK(screw.socketHeadDiameter > screw.clearanceCoarse);
        CHECK(screw.nutWidth > screw.nominal);
    }

    // M3..M12: the tap drill approximates nominal − pitch (ISO 261/ISO 2306).
    for (const MetricScrew& screw : screws) {
        if (screw.nominal >= 3.0) {
            CHECK(std::abs(screw.tapDrill - (screw.nominal - screw.pitch)) <= 0.06);
        }
    }

    // The table is sorted by nominal diameter.
    for (std::size_t i = 1; i < screws.size(); ++i) {
        CHECK(screws[i - 1].nominal < screws[i].nominal);
    }
}

TEST_CASE("metric screw lookup by size label", "[features][holes]") {
    const MetricScrew* m3 = findMetricScrew("M3");
    REQUIRE(m3 != nullptr);
    CHECK(m3->clearanceMedium == 3.4);
    CHECK(findMetricScrew("M7") == nullptr);
}

TEST_CASE("clearance hole diameter by fit", "[features][holes]") {
    const MetricScrew* m4 = findMetricScrew("M4");
    REQUIRE(m4 != nullptr);
    CHECK(clearanceDiameter(*m4, ClearanceFit::coarse) == 4.8);
}

TEST_CASE("hexagon corner-to-corner width", "[features][holes]") {
    CHECK(hexCornerWidth(5.5) == Approx(6.3509).margin(1e-4));
}
