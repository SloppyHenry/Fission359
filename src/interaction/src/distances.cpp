// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/distances.h"

#include <algorithm>
#include <cmath>

namespace fission::interaction {

namespace {

// The axis a side runs along (0 = x, 1 = y), whether it is the upper side, and the other axis.
struct SideInfo {
    std::size_t axis;
    bool upper;
    std::size_t across;
};

SideInfo infoOf(Side side) {
    switch (side) {
    case Side::left:
        return {0, false, 1};
    case Side::right:
        return {0, true, 1};
    case Side::front:
        return {1, false, 0};
    case Side::back:
        return {1, true, 0};
    }
    return {0, false, 1};
}

// How far two ranges overlap (negative: how far apart they are).
double overlap(double lowA, double highA, double lowB, double highB) {
    return std::min(highA, highB) - std::max(lowA, lowB);
}

} // namespace

std::vector<Gap> nearestGaps(const Box3& mover,
                             const std::vector<std::pair<std::string, Box3>>& others,
                             double bedSizeX, double bedSizeY) {
    std::vector<Gap> gaps;
    const double bedHalf[2] = {bedSizeX / 2.0, bedSizeY / 2.0};
    for (const Side side : {Side::left, Side::right, Side::front, Side::back}) {
        const SideInfo info = infoOf(side);
        const std::size_t a = info.axis;
        const std::size_t c = info.across;

        // The nearest body in this direction.
        const std::pair<std::string, Box3>* nearest = nullptr;
        double nearestDistance = 0.0;
        for (const auto& candidate : others) {
            const Box3& box = candidate.second;
            if (overlap(mover.low[c], mover.high[c], box.low[c], box.high[c]) <= 0.0) {
                continue; // beside it, not in this direction
            }
            const double distance =
                info.upper ? box.low[a] - mover.high[a] : mover.low[a] - box.high[a];
            if (distance < 0.0) {
                continue; // overlaps the body along this axis: not a gap
            }
            if (nearest == nullptr || distance < nearestDistance) {
                nearest = &candidate;
                nearestDistance = distance;
            }
        }

        Gap gap;
        gap.side = side;
        Vec3 from{};
        Vec3 to{};
        if (nearest != nullptr) {
            const Box3& box = nearest->second;
            gap.distance = nearestDistance;
            gap.otherBody = nearest->first;
            // The band lies in the middle of the overlap across, and half way up the overlap in
            // height (between the two where they do not overlap in height).
            const double across =
                (std::max(mover.low[c], box.low[c]) + std::min(mover.high[c], box.high[c])) / 2.0;
            const double height =
                (std::max(mover.low[2], box.low[2]) + std::min(mover.high[2], box.high[2])) / 2.0;
            from[a] = info.upper ? mover.high[a] : mover.low[a];
            to[a] = info.upper ? box.low[a] : box.high[a];
            from[c] = to[c] = across;
            from[2] = to[2] = height;
        } else {
            // The edge of the bed; the body must be inside it.
            const double edge = info.upper ? bedHalf[a] : -bedHalf[a];
            const double distance = info.upper ? edge - mover.high[a] : mover.low[a] - edge;
            if (distance < 0.0) {
                continue;
            }
            gap.distance = distance;
            gap.toBed = true;
            from[a] = info.upper ? mover.high[a] : mover.low[a];
            to[a] = edge;
            from[c] = to[c] = (mover.low[c] + mover.high[c]) / 2.0;
            from[2] = to[2] = kBedBandHeight;
        }
        gap.distance = cleanedCoordinate(gap.distance);
        for (std::size_t i = 0; i < 3; ++i) {
            from[i] = cleanedCoordinate(from[i]);
            to[i] = cleanedCoordinate(to[i]);
        }
        gap.from = from;
        gap.to = to;
        gaps.push_back(std::move(gap));
    }
    return gaps;
}

std::optional<Vec3> offsetForDistance(const Gap& gap, double wanted) {
    if (!std::isfinite(wanted) || wanted < 0.0) {
        return std::nullopt;
    }
    const SideInfo info = infoOf(gap.side);
    // Towards the other thing the distance shrinks, away from it it grows.
    Vec3 offset{};
    offset[info.axis] = info.upper ? gap.distance - wanted : wanted - gap.distance;
    return offset;
}

} // namespace fission::interaction
