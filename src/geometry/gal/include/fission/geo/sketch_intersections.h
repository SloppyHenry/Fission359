// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <vector>

#include "fission/geo/outline.h"

namespace fission::geo {

enum class SketchCurveKind { line, circle, arc, spline };

/// One curve of a sketch for `GeometryKernel::sketchIntersections`, described by its points in
/// plane coordinates (mm), exactly as a sketch element holds them:
///  - line:   {start, end}
///  - circle: {middle, a point on the circle}
///  - arc:    {middle, start, end}: counter-clockwise from start to end (end gives the direction)
///  - spline: {the points it goes through} (AP-2.1d; a last point equal to the first closes it)
struct SketchCurve {
    SketchCurveKind kind = SketchCurveKind::line;
    std::vector<Point2d> points;
};

/// Where a curve meets another one. The parameter says where on the curve:
///  - line:   the distance from its start (mm)
///  - circle: the angle (radians, [0, 2π)) counter-clockwise from the direction middle -> the
///            second point
///  - arc:    the angle counter-clockwise from its start (radians); inside the arc 0 to its sweep
///  - spline: the length of the chords of its points up to there (mm), the parameter of the curve
struct SketchCrossing {
    double parameter = 0.0;
    Point2d point;
    std::size_t other = 0;       ///< index of the other curve
    double otherParameter = 0.0; ///< where it is on the other curve
};

/// Points nearer than this (mm) are one point: crossings are merged, curves meet.
inline constexpr double kSketchCrossingTolerance = 1e-6;

} // namespace fission::geo
