// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "fission/geo/solid.h"

namespace fission::geo {

/// What a result of a risky operation must look like to be believed. A valid solid proves nothing:
/// the kernel can hand back a valid body that is simply wrong (hollowing a rounded box).
struct Expectation {
    double volumeMin = 0.0; ///< mm³, inclusive.
    double volumeMax = 0.0; ///< mm³, inclusive.
    int solids = 1;         ///< Bodies the result must have.
    int shells = 0;         ///< Closed surfaces it must have; 0 = not checked.
};

struct PlausibilityReport {
    bool plausible = false;
    std::string reason; ///< For the log: what was wrong first; empty when plausible.
};

/// Judges `after` (the report of the result) against what was expected. Kernel independent: the
/// numbers come from `GeometryKernel::measure`. `before` is the report of the body the operation
/// started from; the result must never be larger than it unless `mayGrow`.
inline PlausibilityReport checkPlausibility(const SolidReport& before, const SolidReport& after,
                                            const Expectation& expected, bool mayGrow = false) {
    const auto fail = [](std::string reason) {
        return PlausibilityReport{false, std::move(reason)};
    };
    if (!after.valid) {
        return fail("the result is not a valid solid");
    }
    if (after.solids != expected.solids) {
        return fail("the result has " + std::to_string(after.solids) + " bodies, expected " +
                    std::to_string(expected.solids));
    }
    if (expected.shells > 0 && after.shells != expected.shells) {
        return fail("the result has " + std::to_string(after.shells) + " shells, expected " +
                    std::to_string(expected.shells));
    }
    if (!(after.volume >= expected.volumeMin) || !(after.volume <= expected.volumeMax)) {
        return fail("the volume " + std::to_string(after.volume) + " is outside " +
                    std::to_string(expected.volumeMin) + " to " +
                    std::to_string(expected.volumeMax));
    }
    if (!mayGrow && after.volume > before.volume * (1.0 + 1e-9)) {
        return fail("the result is larger than the body it was made from");
    }
    return {true, {}};
}

} // namespace fission::geo
