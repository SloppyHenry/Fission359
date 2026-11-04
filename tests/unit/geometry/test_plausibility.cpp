// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The plausibility check (AP-2.6): a result that is valid but wrong is not believed.

#include <catch2/catch_test_macros.hpp>

#include "fission/geo/plausibility.h"

namespace geo = fission::geo;

namespace {

geo::SolidReport report(double volume, int solids = 1, int shells = 1, bool valid = true) {
    geo::SolidReport made;
    made.volume = volume;
    made.solids = solids;
    made.shells = shells;
    made.valid = valid;
    return made;
}

} // namespace

TEST_CASE("plausibility: a result inside the expected range is believed",
          "[geometry][plausibility]") {
    const geo::Expectation expected{900.0, 1100.0, 1, 1};
    CHECK(geo::checkPlausibility(report(8000.0), report(1000.0), expected).plausible);
    CHECK(geo::checkPlausibility(report(8000.0), report(900.0), expected).plausible);
    CHECK(geo::checkPlausibility(report(8000.0), report(1100.0), expected).plausible);
}

TEST_CASE("plausibility: every known kind of wrong result is caught", "[geometry][plausibility]") {
    const geo::Expectation expected{900.0, 1100.0, 1, 1};
    const geo::SolidReport before = report(8000.0);
    // Valid, but the volume is not what a hollow body can have (the case of the spike).
    CHECK_FALSE(geo::checkPlausibility(before, report(7999.0), expected).plausible);
    CHECK_FALSE(geo::checkPlausibility(before, report(100.0), expected).plausible);
    // Not valid.
    const auto invalid = geo::checkPlausibility(before, report(1000.0, 1, 1, false), expected);
    CHECK_FALSE(invalid.plausible);
    CHECK_FALSE(invalid.reason.empty());
    // Fell apart or came back with the wrong number of shells.
    CHECK_FALSE(geo::checkPlausibility(before, report(1000.0, 2, 1), expected).plausible);
    CHECK_FALSE(geo::checkPlausibility(before, report(1000.0, 1, 2), expected).plausible);
    // Not checked when not asked for.
    CHECK(
        geo::checkPlausibility(before, report(1000.0, 1, 5), geo::Expectation{900.0, 1100.0, 1, 0})
            .plausible);
}

TEST_CASE("plausibility: a result larger than its source is refused unless it may grow",
          "[geometry][plausibility]") {
    const geo::Expectation expected{0.0, 1e9, 1, 0};
    CHECK_FALSE(geo::checkPlausibility(report(100.0), report(200.0), expected).plausible);
    CHECK(geo::checkPlausibility(report(100.0), report(200.0), expected, true).plausible);
}
