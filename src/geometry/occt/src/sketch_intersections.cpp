// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Where the curves of a sketch cross (AP-2.1e1). Lines and circles (and arcs, which are pieces of
// circles) are crossed with plain maths, which is exact and finds a touch; anything with a spline
// in it goes to OpenCASCADE (`Geom2dAPI_InterCurveCurve`) on the interpolated curve. Every pair
// is first crossed on the whole line or circle, then the crossings outside the drawn pieces are
// dropped.

#include "fission/geo/occt/sketch_intersections.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "fission/geo/error.h"

#include "sketch_spline_curve.h"
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dInt_GInter.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Line.hxx>
#include <IntRes2d_IntersectionPoint.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Circ2d.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt2d.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kTol = kSketchCrossingTolerance;

double distance(const Point2d& a, const Point2d& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

// A curve ready to be crossed: its carrier (the whole line or circle, or the spline) and what
// part of it is drawn.
struct Carrier {
    enum class Shape { line, circle, spline } shape = Shape::line;
    SketchCurveKind kind = SketchCurveKind::line;
    // line (also a spline of two points)
    Point2d origin;
    Point2d way; ///< unit vector
    double length = 0.0;
    // circle and arc
    Point2d centre;
    double radius = 0.0;
    double start = 0.0; ///< angle of parameter 0
    double sweep = 0.0; ///< the part drawn (2π for a circle)
    // spline
    InterpolatedSpline spline;
    double total = 0.0;

    // the drawn part, in the parameter of the carrier
    bool drawn(double parameter) const {
        switch (shape) {
        case Shape::line:
        case Shape::spline:
            return parameter >= -kTol && parameter <= length + kTol;
        case Shape::circle:
            return kind == SketchCurveKind::circle ||
                   (parameter >= -kTol / radius && parameter <= sweep + kTol / radius);
        }
        return false;
    }

    Point2d at(double parameter) const {
        if (shape == Shape::circle) {
            return {centre.x + radius * std::cos(start + parameter),
                    centre.y + radius * std::sin(start + parameter)};
        }
        return {origin.x + way.x * parameter, origin.y + way.y * parameter};
    }
};

void requireFinite(const std::vector<Point2d>& points) {
    for (const Point2d& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "sketchIntersections: a point is not finite", "point");
        }
    }
}

// Angle of `p` seen from the centre, counter-clockwise from the carrier's start, in [0, 2π).
double angleOn(const Carrier& c, const Point2d& p) {
    double a = std::atan2(p.y - c.centre.y, p.x - c.centre.x) - c.start;
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}

Carrier carrierOf(const SketchCurve& curve) {
    Carrier c;
    c.kind = curve.kind;
    const std::vector<Point2d>& p = curve.points;
    requireFinite(p);
    switch (curve.kind) {
    case SketchCurveKind::line: {
        if (p.size() != 2 || !(distance(p[0], p[1]) > 0.0)) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "sketchIntersections: a line needs two different points", "line");
        }
        c.shape = Carrier::Shape::line;
        c.origin = p[0];
        c.length = distance(p[0], p[1]);
        c.way = {(p[1].x - p[0].x) / c.length, (p[1].y - p[0].y) / c.length};
        break;
    }
    case SketchCurveKind::circle:
    case SketchCurveKind::arc: {
        const std::size_t count = curve.kind == SketchCurveKind::circle ? 2 : 3;
        if (p.size() != count || !(distance(p[0], p[1]) > 0.0)) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "sketchIntersections: a circle or arc needs a radius", "circle");
        }
        c.shape = Carrier::Shape::circle;
        c.centre = p[0];
        c.radius = distance(p[0], p[1]);
        c.start = std::atan2(p[1].y - p[0].y, p[1].x - p[0].x);
        c.sweep = kTwoPi;
        if (curve.kind == SketchCurveKind::arc) {
            if (!(distance(p[0], p[2]) > 0.0)) {
                throw GeometryError(GeometryErrorCode::invalidInput,
                                    "sketchIntersections: an arc needs an end", "arc");
            }
            const double sweep = angleOn(c, p[2]);
            c.sweep = sweep > 1e-12 ? sweep : kTwoPi; // start and end in one direction: all round
        }
        break;
    }
    case SketchCurveKind::spline: {
        c.spline = interpolateSketchSpline(p);
        if (!c.spline.curve) { // two points: straight
            const Point2d& a = c.spline.points[0];
            const Point2d& b = c.spline.points[1];
            c.shape = Carrier::Shape::line;
            c.origin = a;
            c.length = distance(a, b);
            c.way = {(b.x - a.x) / c.length, (b.y - a.y) / c.length};
        } else {
            c.shape = Carrier::Shape::spline;
            c.total = c.spline.parameters.back();
            c.length = c.total;
        }
        break;
    }
    }
    return c;
}

struct Hit {
    Point2d point;
    double a = 0.0; ///< parameter on the first carrier
    double b = 0.0; ///< on the second
};

double parameterOf(const Carrier& c, const Point2d& p) {
    if (c.shape == Carrier::Shape::circle) {
        return angleOn(c, p);
    }
    return (p.x - c.origin.x) * c.way.x + (p.y - c.origin.y) * c.way.y;
}

void crossLines(const Carrier& l1, const Carrier& l2, std::vector<Hit>& out) {
    const double cross = l1.way.x * l2.way.y - l1.way.y * l2.way.x;
    if (std::abs(cross) < 1e-12) {
        return; // parallel: apart, or on top of each other (no crossing)
    }
    const double dx = l2.origin.x - l1.origin.x;
    const double dy = l2.origin.y - l1.origin.y;
    const double s = (dx * l2.way.y - dy * l2.way.x) / cross;
    const double t = (dx * l1.way.y - dy * l1.way.x) / cross;
    out.push_back({l1.at(s), s, t});
}

void crossLineCircle(const Carrier& line, const Carrier& circle, bool lineFirst,
                     std::vector<Hit>& out) {
    const double fx = circle.centre.x - line.origin.x;
    const double fy = circle.centre.y - line.origin.y;
    const double foot = fx * line.way.x + fy * line.way.y;
    const double height = std::abs(fx * line.way.y - fy * line.way.x);
    if (height > circle.radius + kTol) {
        return;
    }
    const auto add = [&](double along) {
        const Point2d p = line.at(along);
        const double onCircle = angleOn(circle, p);
        out.push_back(lineFirst ? Hit{p, along, onCircle} : Hit{p, onCircle, along});
    };
    if (std::abs(height - circle.radius) <= kTol) { // a touch
        add(foot);
        return;
    }
    const double half = std::sqrt(circle.radius * circle.radius - height * height);
    add(foot - half);
    add(foot + half);
}

void crossCircles(const Carrier& c1, const Carrier& c2, std::vector<Hit>& out) {
    const double d = distance(c1.centre, c2.centre);
    if (d < 1e-12) {
        return; // the same middle: apart, or on top of each other
    }
    const double r1 = c1.radius;
    const double r2 = c2.radius;
    if (d > r1 + r2 + kTol || d < std::abs(r1 - r2) - kTol) {
        return;
    }
    const double ux = (c2.centre.x - c1.centre.x) / d;
    const double uy = (c2.centre.y - c1.centre.y) / d;
    const double along = (d * d + r1 * r1 - r2 * r2) / (2.0 * d);
    const auto add = [&](const Point2d& p) { out.push_back({p, angleOn(c1, p), angleOn(c2, p)}); };
    const bool touch = std::abs(d - (r1 + r2)) <= kTol || std::abs(d - std::abs(r1 - r2)) <= kTol;
    if (touch) {
        // the point on the line between the middles, on the side of the nearer circle
        const bool outside = std::abs(d - (r1 + r2)) <= kTol;
        const double s = outside ? r1 : (r1 > r2 ? r1 : -r1);
        add({c1.centre.x + ux * s, c1.centre.y + uy * s});
        return;
    }
    const double h = std::sqrt(std::max(0.0, r1 * r1 - along * along));
    const Point2d base{c1.centre.x + ux * along, c1.centre.y + uy * along};
    add({base.x - uy * h, base.y + ux * h});
    add({base.x + uy * h, base.y - ux * h});
}

occ::handle<Geom2d_Curve> geometryOf(const Carrier& c) {
    switch (c.shape) {
    case Carrier::Shape::line:
        return new Geom2d_Line(gp_Pnt2d(c.origin.x, c.origin.y), gp_Dir2d(c.way.x, c.way.y));
    case Carrier::Shape::circle:
        return new Geom2d_Circle(gp_Circ2d(gp_Ax2d(gp_Pnt2d(c.centre.x, c.centre.y),
                                                   gp_Dir2d(std::cos(c.start), std::sin(c.start))),
                                           c.radius));
    case Carrier::Shape::spline:
        return c.spline.curve;
    }
    return {};
}

// A spline against anything: OpenCASCADE finds the points; the parameter on a circle is read off
// the point (the circle carries its own start).
void crossWithSpline(const Carrier& first, const Carrier& second, std::vector<Hit>& out) {
    try {
        Geom2dAPI_InterCurveCurve cross(geometryOf(first), geometryOf(second), kTol);
        const Geom2dInt_GInter& found = cross.Intersector();
        for (int i = 1; i <= cross.NbPoints(); ++i) {
            const gp_Pnt2d point = cross.Point(i);
            const Point2d p{point.X(), point.Y()};
            const IntRes2d_IntersectionPoint& at = found.Point(i);
            const double a =
                first.shape == Carrier::Shape::spline ? at.ParamOnFirst() : parameterOf(first, p);
            const double b = second.shape == Carrier::Shape::spline ? at.ParamOnSecond()
                                                                    : parameterOf(second, p);
            out.push_back({p, a, b});
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "sketchIntersections: the kernel failed");
    }
}

// All crossings of the two carriers on the whole line / circle.
std::vector<Hit> crossCarriers(const Carrier& a, const Carrier& b) {
    std::vector<Hit> out;
    if (a.shape == Carrier::Shape::spline || b.shape == Carrier::Shape::spline) {
        crossWithSpline(a, b, out);
    } else if (a.shape == Carrier::Shape::line && b.shape == Carrier::Shape::line) {
        crossLines(a, b, out);
    } else if (a.shape == Carrier::Shape::line) {
        crossLineCircle(a, b, true, out);
    } else if (b.shape == Carrier::Shape::line) {
        crossLineCircle(b, a, false, out);
    } else {
        crossCircles(a, b, out);
    }
    return out;
}

// A parameter just outside an end is the end.
double clampedTo(const Carrier& c, double parameter) {
    switch (c.shape) {
    case Carrier::Shape::line:
    case Carrier::Shape::spline:
        return std::clamp(parameter, 0.0, c.length);
    case Carrier::Shape::circle:
        if (c.kind == SketchCurveKind::arc) {
            if (parameter > kTwoPi - kTol / c.radius) {
                return 0.0; // just before the start, round the circle
            }
            return std::clamp(parameter, 0.0, c.sweep);
        }
        return parameter;
    }
    return parameter;
}

bool drawnHere(const Carrier& c, double parameter) {
    if (c.shape == Carrier::Shape::circle && c.kind == SketchCurveKind::arc &&
        parameter > kTwoPi - kTol / c.radius) {
        return true; // at the start, seen from the other side
    }
    return c.drawn(parameter);
}

void addCrossing(std::vector<SketchCrossing>& list, SketchCrossing crossing) {
    for (const SketchCrossing& have : list) {
        if (distance(have.point, crossing.point) <= kTol) {
            return;
        }
    }
    list.push_back(crossing);
}

} // namespace

std::vector<std::vector<SketchCrossing>>
sketchIntersections(const std::vector<SketchCurve>& curves) {
    std::vector<Carrier> carriers;
    carriers.reserve(curves.size());
    for (const SketchCurve& curve : curves) {
        carriers.push_back(carrierOf(curve));
    }
    std::vector<std::vector<SketchCrossing>> result(curves.size());
    for (std::size_t i = 0; i < carriers.size(); ++i) {
        for (std::size_t j = i + 1; j < carriers.size(); ++j) {
            for (const Hit& hit : crossCarriers(carriers[i], carriers[j])) {
                if (!drawnHere(carriers[i], hit.a) || !drawnHere(carriers[j], hit.b)) {
                    continue;
                }
                const double a = clampedTo(carriers[i], hit.a);
                const double b = clampedTo(carriers[j], hit.b);
                addCrossing(result[i], {a, hit.point, j, b});
                addCrossing(result[j], {b, hit.point, i, a});
            }
        }
    }
    for (std::vector<SketchCrossing>& list : result) {
        std::ranges::sort(list, {}, &SketchCrossing::parameter);
    }
    return result;
}

std::vector<SketchCrossing> sketchCarrierCrossings(const SketchCurve& carrier,
                                                   const std::vector<SketchCurve>& others) {
    if (carrier.kind == SketchCurveKind::spline) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "sketchCarrierCrossings: a spline has no carrier", "spline");
    }
    const Carrier own = carrierOf(carrier);
    std::vector<SketchCrossing> result;
    for (std::size_t j = 0; j < others.size(); ++j) {
        const Carrier other = carrierOf(others[j]);
        for (const Hit& hit : crossCarriers(own, other)) {
            if (!drawnHere(other, hit.b)) {
                continue;
            }
            addCrossing(result, {hit.a, hit.point, j, clampedTo(other, hit.b)});
        }
    }
    std::ranges::sort(result, {}, &SketchCrossing::parameter);
    return result;
}

} // namespace fission::geo::occt
