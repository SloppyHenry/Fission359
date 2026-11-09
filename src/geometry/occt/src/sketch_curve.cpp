// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The curve of a sketch spline (AP-2.1d): OpenCASCADE interpolates the points with a cubic
// B-spline (periodic when the curve closes on its first point), and the curve is cut into short
// lines wherever it bends away from them.

#include "fission/geo/occt/sketch_curve.h"

#include <algorithm>
#include <cmath>

#include "fission/geo/error.h"

#include <Geom2dAPI_Interpolate.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec2d.hxx>

namespace fission::geo::occt {

namespace {

constexpr int kMaxDepth = 18;

double distance(const Point2d& a, const Point2d& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

Point2d pointOf(const gp_Pnt2d& point) {
    return {point.X(), point.Y()};
}

// How far `p` is from the segment a-b.
double distanceToSegment(const Point2d& p, const Point2d& a, const Point2d& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double lengthSquared = dx * dx + dy * dy;
    double t = lengthSquared > 0.0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

class Sampler {
public:
    Sampler(const Geom2d_BSplineCurve& curve, double tolerance)
        : curve_(curve), tolerance_(tolerance) {}

    // Appends the points of the curve from parameter `t0` (point `p0`, already in the chain) to
    // `t1` (point `p1`, appended last), cutting the span in two while it strays from its chord.
    void span(double t0, const Point2d& p0, double t1, const Point2d& p1, int depth,
              std::vector<Point2d>& out) const {
        if (depth < kMaxDepth && strays(t0, p0, t1, p1)) {
            const double tm = 0.5 * (t0 + t1);
            const Point2d pm = at(tm);
            span(t0, p0, tm, pm, depth + 1, out);
            span(tm, pm, t1, p1, depth + 1, out);
            return;
        }
        out.push_back(p1);
    }

    Point2d at(double t) const { return pointOf(curve_.Value(t)); }

private:
    // The curve between the two ends is looked at in three more places; half the tolerance is
    // the allowance, so that what lies between those places stays within the whole.
    bool strays(double t0, const Point2d& p0, double t1, const Point2d& p1) const {
        for (const double share : {0.25, 0.5, 0.75}) {
            const Point2d p = at(t0 + (t1 - t0) * share);
            if (distanceToSegment(p, p0, p1) > 0.5 * tolerance_) {
                return true;
            }
        }
        return false;
    }

    const Geom2d_BSplineCurve& curve_;
    double tolerance_;
};

Point2d unitDirection(const Geom2d_BSplineCurve& curve, double t) {
    gp_Pnt2d point;
    gp_Vec2d way;
    curve.D1(t, point, way);
    const double length = way.Magnitude();
    if (!(length > 1e-12) || !std::isfinite(length)) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "sketchSpline: no direction");
    }
    return {way.X() / length, way.Y() / length};
}

} // namespace

SketchSplineCurve sketchSpline(const std::vector<Point2d>& through, double tolerance) {
    if (!(tolerance > 0.0) || !std::isfinite(tolerance)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "sketchSpline: tolerance",
                            "tolerance");
    }
    for (const Point2d& point : through) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "sketchSpline: a point is not finite", "point");
        }
    }
    // Following points that are one are merged; the last one on the first closes the curve.
    std::vector<Point2d> points;
    for (const Point2d& point : through) {
        if (points.empty() || distance(points.back(), point) > kSketchSplineMergeDistance) {
            points.push_back(point);
        }
    }
    const bool closed =
        points.size() >= 2 && distance(points.front(), points.back()) <= kSketchSplineMergeDistance;
    if (closed) {
        points.pop_back();
    }
    const std::size_t count = points.size();
    if (count < (closed ? 3U : 2U)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "sketchSpline: too few points",
                            "points");
    }

    SketchSplineCurve made;
    made.closed = closed;
    if (count == 2) { // a straight curve
        const double length = distance(points[0], points[1]);
        const Point2d way{(points[1].x - points[0].x) / length,
                          (points[1].y - points[0].y) / length};
        made.polyline = {points[0], points[1]};
        made.startTangent = way;
        made.endTangent = way;
        return made;
    }

    try {
        // Parameters by the length of the chords (what a person expects of a curve through points).
        const int n = static_cast<int>(count);
        std::vector<double> parameters(count + (closed ? 1U : 0U), 0.0);
        for (std::size_t i = 1; i < parameters.size(); ++i) {
            parameters[i] = parameters[i - 1] + distance(points[i - 1], points[i % count]);
        }
        occ::handle<NCollection_HArray1<gp_Pnt2d>> ends = new NCollection_HArray1<gp_Pnt2d>(1, n);
        occ::handle<NCollection_HArray1<double>> times =
            new NCollection_HArray1<double>(1, static_cast<int>(parameters.size()));
        for (int i = 0; i < n; ++i) {
            ends->SetValue(i + 1, gp_Pnt2d(points[static_cast<std::size_t>(i)].x,
                                           points[static_cast<std::size_t>(i)].y));
        }
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            times->SetValue(static_cast<int>(i) + 1, parameters[i]);
        }
        Geom2dAPI_Interpolate interpolation(ends, times, closed, kSketchSplineMergeDistance);
        interpolation.Perform();
        if (!interpolation.IsDone()) {
            throw GeometryError(GeometryErrorCode::kernelFailure, "sketchSpline: no interpolation");
        }
        const occ::handle<Geom2d_BSplineCurve> curve = interpolation.Curve();
        const Sampler sampler(*curve, tolerance);

        // The curve must really go through the points: never a curve that is quietly different.
        for (std::size_t i = 0; i < count; ++i) {
            if (distance(sampler.at(parameters[i]), points[i]) > 1e-6) {
                throw GeometryError(GeometryErrorCode::kernelFailure,
                                    "sketchSpline: the curve misses a point");
            }
        }
        made.polyline.push_back(points.front());
        for (std::size_t i = 0; i + 1 < parameters.size(); ++i) {
            sampler.span(parameters[i], points[i], parameters[i + 1], points[(i + 1) % count], 0,
                         made.polyline);
        }
        made.startTangent = unitDirection(*curve, parameters.front());
        made.endTangent = unitDirection(*curve, parameters.back());
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "sketchSpline: the kernel failed");
    }
    return made;
}

} // namespace fission::geo::occt
