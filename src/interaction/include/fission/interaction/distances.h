// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Distance measures (Abstandsmaße, AP-1.6d): how far a body is from the nearest bodies and from
// the edge of the print bed, edge to edge, in the four directions of the bed. And the move that
// makes one of those distances a wanted number ("5 mm from the edge"). Pure geometry (mm), no
// window: the controller of the interface uses it, and the tests check it.
//
// The boxes are the axis-parallel boxes round the bodies (a turned body counts by the box round
// it, as in snapping).

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fission/interaction/snap.h"

namespace fission::interaction {

/// The axis-parallel box round a body (mm).
struct Box3 {
    Vec3 low{};
    Vec3 high{};
};

/// The directions of the bed: -X, +X, -Y (front), +Y (back).
enum class Side { left, right, front, back };

/// One measure: the free space between the body and the next thing in one direction.
struct Gap {
    Side side = Side::left;
    double distance = 0.0; ///< mm, 0 or more; from the edge of the body to the edge beside it
    Vec3 from{};           ///< the end of the band on the body (mm, in the world)
    Vec3 to{};             ///< the end of the band on the other body or the edge of the bed
    bool toBed = false;    ///< the other end is the edge of the bed, not a body
    std::string otherBody; ///< empty at the edge of the bed
};

/// Height above the bed (mm) at which the band to the edge of the bed is drawn.
inline constexpr double kBedBandHeight = 0.2;

/// The measures of `mover` in each direction: to the nearest of `others` that lies in that
/// direction (its footprint overlaps the footprint of the body across that direction, and it does
/// not overlap the body along it), or else to the edge of the bed (origin in the middle, size
/// `bedSizeX` x `bedSizeY`). A direction has no measure when the body reaches over the edge of the
/// bed. At most four, in the order left, right, front, back.
std::vector<Gap> nearestGaps(const Box3& mover,
                             const std::vector<std::pair<std::string, Box3>>& others,
                             double bedSizeX, double bedSizeY);

/// The move (mm, along X or along Y) that makes the distance of `gap` the number `wanted`; empty
/// for a number below 0 or no number (a body is not pushed into another by a measure).
std::optional<Vec3> offsetForDistance(const Gap& gap, double wanted);

} // namespace fission::interaction
