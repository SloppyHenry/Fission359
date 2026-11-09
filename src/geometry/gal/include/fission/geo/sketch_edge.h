// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/sketch_intersections.h"
#include "fission/geo/types.h"

namespace fission::geo {

/// A plane of a sketch as the kernel needs it (`GeometryKernel::projectEdgeToSketch`): a point and
/// two perpendicular unit vectors; the normal is `u × v`.
struct SketchFrame {
    Point3d origin;
    Point3d u{1.0, 0.0, 0.0};
    Point3d v{0.0, 1.0, 0.0};
};

/// Why an edge of a body could not be taken over exactly (AP-2.1f, decision 46).
enum class SketchEdgeStatus {
    ok,            ///< `kind` and `points` hold the exact projection.
    perpendicular, ///< The edge runs along the plane's normal: it would be a point.
    notExact,      ///< A line, circle or arc would not be exact (a tilted circle is an ellipse, a
                   ///< free-form edge, a circle seen edge-on): nothing is made up.
};

/// An edge of a body projected perpendicularly onto the plane of a sketch, in plane coordinates
/// (mm), described like a sketch element (`SketchCurve`): line {start, end}; circle {middle, a
/// point on it}; arc {middle, start, end} counter-clockwise as seen on the plane.
struct SketchEdgeShape {
    SketchEdgeStatus status = SketchEdgeStatus::notExact;
    SketchCurveKind kind = SketchCurveKind::line;
    std::vector<Point2d> points;
};

/// How near to perpendicular a circle's axis must be to the plane (a sine, no unit): a circle only
/// stays a circle when its axis is perpendicular to the plane, to within rounding errors.
inline constexpr double kSketchEdgeAxisTolerance = 1e-9;

} // namespace fission::geo
