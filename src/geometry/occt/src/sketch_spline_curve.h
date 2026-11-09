// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The cubic B-spline through the points of a sketch spline, for the parts of the OCCT layer that
// need the curve itself (the chain of lines of `sketchSpline`, the crossings of
// `sketchIntersections`). Private to the OCCT layer: it names OCCT types.

#pragma once

#include <vector>

#include "fission/geo/outline.h"

#include <Geom2d_BSplineCurve.hxx>

namespace fission::geo::occt {

struct InterpolatedSpline {
    /// The points the curve goes through after merging following equal points; a closed curve
    /// does not repeat its first point at the end.
    std::vector<Point2d> points;
    /// The parameter of each point (the length of the chords up to it); a closed curve has one
    /// more, for the first point reached again.
    std::vector<double> parameters;
    /// Empty for a straight curve of two points.
    occ::handle<Geom2d_BSplineCurve> curve;
    bool closed = false;
};

/// Throws GeometryError (invalidInput for non-finite numbers or too few points, kernelFailure).
InterpolatedSpline interpolateSketchSpline(const std::vector<Point2d>& through);

} // namespace fission::geo::occt
