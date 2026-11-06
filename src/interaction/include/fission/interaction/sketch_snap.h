// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The pointer on a sketch plane (AP-2.1b, ADR-0013): where a ray from the eye meets the plane,
// and where the pointer catches on the sketch (end points, middles, centres, lines up with other
// points, grid). Pure maths in plane coordinates (mm), no window.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/geo/types.h"

namespace fission::interaction {

/// A line of sight: a point and a direction (any length) in the world (mm).
struct PlaneRay {
    geo::Point3d origin;
    geo::Point3d direction;
};

/// The point of the plane the ray meets, in plane coordinates. Nothing when the ray runs parallel
/// to the plane (or has no direction) or the plane lies behind the eye.
std::optional<core::SketchPoint> planePoint(const PlaneRay& ray, const core::SketchPlane& plane);

/// What the pointer caught on, in the order of precedence: an end point or a point of the sketch
/// beats a middle (of a line, the centre of a circle or an arc), which beats lining up with
/// another point, which beats the grid.
enum class SketchSnapKind { none, point, middle, alignment, grid };

/// A dashed aid line from the point it lines up with to the caught position (plane coordinates).
struct SketchGuide {
    core::SketchPoint from;
    core::SketchPoint to;

    bool operator==(const SketchGuide&) const = default;
};

struct SketchSnapSettings {
    /// How far (screen pixels) the pointer may be from a point to catch on it.
    double radiusPixels = 10.0;
    /// The zoom of the view; with it the radius is worked out in mm.
    double pixelsPerMillimetre = 2.0;
    /// The distance (mm) of the grid lines, 0 for no grid.
    double gridStep = 1.0;
    /// False when the person holds the key that switches snapping off: the position stays as it is.
    bool enabled = true;
};

struct SketchSnap {
    core::SketchPoint position; ///< Where the point goes.
    SketchSnapKind kind = SketchSnapKind::none;
    /// The element it caught on (a point, a middle, a centre), empty for alignment and grid.
    std::string element;
    /// One guide for each way it lines up (horizontal, vertical), to be drawn dashed.
    std::vector<SketchGuide> guides;
};

/// The distance (mm) of the grid lines drawn on a sketch plane: `baseStep` (the grid setting of
/// the bed), made coarser (2, 5, 10, 20, ... times) until neighbouring lines are at least
/// `minPixels` apart on the screen. 0 for a `baseStep` or zoom that is not positive.
double sketchGridStep(double baseStep, double pixelsPerMillimetre, double minPixels = 6.0);

/// Catches the pointer (plane coordinates) on the elements of a sketch. Construction lines count
/// like the others. With nothing to catch on and no grid the position is the pointer's.
SketchSnap sketchSnap(const core::SketchPoint& pointer,
                      const std::vector<core::SketchElement>& elements,
                      const SketchSnapSettings& settings);

} // namespace fission::interaction
