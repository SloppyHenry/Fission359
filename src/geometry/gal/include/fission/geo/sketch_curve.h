// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/outline.h"

namespace fission::geo {

/// The curve of a sketch spline (AP-2.1d): a smooth cubic curve through points, as the kernel
/// calculates it. Coordinates are plane coordinates (u as x, v as y), in millimetres.
struct SketchSplineCurve {
    /// The curve as a chain of short lines. Every point the curve goes through is a vertex of the
    /// chain, exactly; between them no point of the curve is further than the asked tolerance from
    /// the chain. A closed curve ends where it starts (the last point equals the first).
    std::vector<Point2d> polyline;
    /// Unit direction of the curve at its start and at its end. For a closed curve they are equal:
    /// there is no corner where it closes.
    Point2d startTangent;
    Point2d endTangent;
    /// The last point given was the first one: a smooth closed curve.
    bool closed = false;
};

/// The least distance (mm) that two following points of a spline may have: nearer ones are one.
inline constexpr double kSketchSplineMergeDistance = 1e-6;

} // namespace fission::geo
