// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Weight and filament length of a part (AP-2.11).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <string>

#include "fission/analysis/material.h"

using Catch::Approx;
using namespace fission::analysis;

TEST_CASE("material: a cube of 20 mm in PLA weighs 9.92 g at most", "[analysis]") {
    const auto pla = findMaterial("pla");
    REQUIRE(pla);
    CHECK(massGrams(20.0 * 20.0 * 20.0, pla->density) == Approx(9.92).epsilon(1e-9));
}

TEST_CASE("material: the table holds the five materials with their densities", "[analysis]") {
    const struct {
        const char* id;
        double density;
    } expected[] = {{"pla", 1.24}, {"petg", 1.27}, {"abs", 1.04}, {"asa", 1.07}, {"tpu", 1.21}};
    CHECK(materials().size() == 5);
    for (const auto& one : expected) {
        const auto found = findMaterial(one.id);
        REQUIRE(found);
        CHECK(found->density == Approx(one.density));
    }
    CHECK_FALSE(findMaterial("wood"));
    CHECK_FALSE(findMaterial(""));
}

TEST_CASE("material: the filament length is the volume over the thread's cross section",
          "[analysis]") {
    const double volume = 8000.0; // the cube of 20 mm
    const double crossSection = std::numbers::pi * 0.875 * 0.875;
    CHECK(filamentLengthMm(volume, kFilament175) == Approx(volume / crossSection).epsilon(1e-12));
    CHECK(filamentLengthMm(volume, kFilament175) == Approx(3326.0).margin(0.5));
    // A thicker thread is shorter, by the square of the thicknesses.
    CHECK(filamentLengthMm(volume, kFilament285) ==
          Approx(filamentLengthMm(volume, kFilament175) * (1.75 * 1.75) / (2.85 * 2.85))
              .epsilon(1e-12));
}

TEST_CASE("material: nothing sensible gives zero, not a crash", "[analysis]") {
    CHECK(filamentLengthMm(1000.0, 0.0) == 0.0);
    CHECK(filamentLengthMm(1000.0, -1.75) == 0.0);
    CHECK(filamentLengthMm(1000.0, std::nan("")) == 0.0);
    CHECK(massGrams(0.0, 1.24) == 0.0);
}
