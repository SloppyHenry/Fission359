// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <variant>
#include <vector>

namespace fission::geo {

/// A point in the plane of a drawing (mm). x to the right, y up.
struct Point2d {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const Point2d&) const = default;
};

struct OutlineLine {
    Point2d from;
    Point2d to;
};

/// A full circle.
struct OutlineCircle {
    Point2d centre;
    double radius = 0.0;
};

/// An arc of a circle, counter-clockwise from `startAngle` to `endAngle` (degrees, 0 = +x).
/// `endAngle` is larger than `startAngle`, and the span is less than 360°.
struct OutlineArc {
    Point2d centre;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
};

/// Any other curve (a spline, an ellipse), as a chain of short lines that stays within 0.005 mm
/// of the true curve.
struct OutlinePolyline {
    std::vector<Point2d> points;
};

using OutlineShape = std::variant<OutlineLine, OutlineCircle, OutlineArc, OutlinePolyline>;

/// The outline of a flat shape for a laser cutter or a drawing program: the edges of a face,
/// seen from outside the body, in millimetres.
struct Outline {
    std::vector<OutlineShape> shapes;
};

} // namespace fission::geo
