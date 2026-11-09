// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The geometry behind the drawing tools of the sketch mode: pure functions in plane coordinates
// (mm), no window and no document. Circles through three points (AP-2.1c1); arcs, the arc that
// joins a line tangentially, the slot and the regular polygon (AP-2.1c2). An arc is always
// {middle, start, end} counter-clockwise (decision 5): what is drawn clockwise has start and end
// swapped.

#pragma once

#include <optional>
#include <vector>

#include "fission/core/sketch.h"

namespace fission::interaction {

/// Anything shorter (a line, a side, a radius) is not drawn: mm.
inline constexpr double kSketchMinimumSize = 0.01;

/// Elements that belong together: they become one command (one undo step). The ids are empty;
/// the document gives them out.
using SketchBatch = std::vector<core::SketchElement>;

/// The circle through three points as {middle, first point}; nothing when they lie on one line (or
/// two are the same), so that no circle fits.
std::optional<core::SketchElement> circleThroughThreePoints(const core::SketchPoint& a,
                                                            const core::SketchPoint& b,
                                                            const core::SketchPoint& c);

/// The arc that starts at `a`, passes `via` and ends at `c`. Nothing when the three lie on one
/// line. Clockwise (as walked from `a` to `c`) the start and end of the element are swapped.
std::optional<core::SketchElement> arcThroughThreePoints(const core::SketchPoint& a,
                                                         const core::SketchPoint& via,
                                                         const core::SketchPoint& c);

/// The shorter arc of the given radius between `a` and `c`, bulging to the left (`side` > 0) or
/// the right (`side` <= 0) of the walk from `a` to `c`. Nothing when the radius is smaller than
/// half the distance between the points (or they are the same).
std::optional<core::SketchElement>
arcOverChord(const core::SketchPoint& a, const core::SketchPoint& c, int side, double radius);

/// The arc round `middle` that starts at `start` and turns by `sweep` radians (positive: counter-
/// clockwise). Nothing when the radius or the turn is (nearly) zero or the turn is a full circle
/// or more.
std::optional<core::SketchElement> arcBySweep(const core::SketchPoint& middle,
                                              const core::SketchPoint& start, double sweep);

/// An arc that leaves `from` along the unit vector `direction` and reaches `to` (or, with a
/// `radius`, the point of that circle nearest to `to`).
struct TangentArc {
    core::SketchElement arc;        ///< {middle, start, end} counter-clockwise
    core::SketchPoint end;          ///< where the walk ends (the end for the chain of lines)
    core::SketchPoint endDirection; ///< unit vector: the way the walk goes on at `end`
    double radius = 0.0;
    bool clockwise = false; ///< walked clockwise: the element has `end` as its start
};
/// Nothing when `to` lies on the line through `from` along `direction` (no arc, only a straight
/// line), is `from`, or when the radius is zero.
std::optional<TangentArc> tangentArc(const core::SketchPoint& from,
                                     const core::SketchPoint& direction,
                                     const core::SketchPoint& to,
                                     std::optional<double> radius = std::nullopt);

/// A slot (obround) as two lines and two half circles, the ends exactly the same points: from the
/// middle `a` of one end to the middle `b` of the other, `width` wide. Empty when the distance or
/// the width is below `kSketchMinimumSize`.
SketchBatch slotElements(const core::SketchPoint& a, const core::SketchPoint& b, double width);

/// A regular polygon as `sides` lines whose last end is exactly the first start. The first
/// corner lies at `angle` (radians) round `middle`, `radius` is that of the circle through the
/// corners. Empty when `sides` < 3 or the radius is below `kSketchMinimumSize`.
SketchBatch polygonLines(const core::SketchPoint& middle, double radius, int sides, double angle);

inline constexpr int kSketchMinimumSides = 3;
inline constexpr int kSketchMaximumSides = 64;
inline constexpr int kSketchDefaultSides = 6;

} // namespace fission::interaction
