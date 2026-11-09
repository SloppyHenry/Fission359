// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <vector>

#include "fission/geo/outline.h"
#include "fission/geo/sketch_intersections.h"

namespace fission::geo {

/// Regions smaller than this (mm²) are slivers of rounding and are dropped (AP-2.1g, decision 51).
inline constexpr double kSketchRegionMinArea = 1e-6;

/// The drawing polygon of a region stays within this distance (mm) of the true boundary (rule 12:
/// round is round).
inline constexpr double kSketchRegionDeflection = 0.005;

/// A curve that bounds a region, and on which side of it the region lies when the curve is
/// followed in the direction it is stored in (`SketchCurve`: a line from its first to its last
/// point, a circle or arc counter-clockwise).
struct SketchRegionSide {
    std::size_t curve = 0; ///< index into the curves given to `sketchRegions`
    bool left = true;      ///< the region is on the left of the curve (inside a circle)
};

/// A stretch of the boundary between two crossings: the curves it lies on (more than one when
/// curves lie on top of each other) and the points of `SketchRegionLoop::polygon` it covers.
struct SketchRegionPiece {
    std::vector<SketchRegionSide> sides;
    std::size_t firstPoint = 0; ///< index into the polygon
    std::size_t lastPoint =
        0; ///< index into the polygon, inclusive; a closed loop ends where it starts
};

/// One closed boundary: the outer one runs counter-clockwise, a hole clockwise (the region is
/// always on the left). `polygon` closes on its first point (the last equals the first).
struct SketchRegionLoop {
    std::vector<Point2d> polygon;
    std::vector<SketchRegionPiece> pieces;
};

/// A closed area of a sketch that no curve crosses (AP-2.1g).
struct SketchRegion {
    SketchRegionLoop outer;
    std::vector<SketchRegionLoop> holes;
    double area = 0.0; ///< mm², exact (not the polygon's)
    Point2d centroid;
    Point2d
        inside; ///< a point well inside the region (the centroid of a bent region may lie outside)
};

struct SketchRegionResult {
    std::vector<SketchRegion> regions;
    /// Areas the kernel found but whose result failed the check; they are left out, never drawn.
    std::size_t rejected = 0;
};

} // namespace fission::geo
