// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/sketch_snap.h"

#include <cmath>

namespace fission::interaction {

namespace {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;

struct Candidate {
    SketchPoint at;
    std::string element;
};

double distance(const SketchPoint& a, const SketchPoint& b) noexcept {
    return std::hypot(a.u - b.u, a.v - b.v);
}

// The point of an arc where it ends: on the circle round the middle, in the direction of the third
// point.
std::optional<SketchPoint> arcEnd(const SketchElement& arc) {
    const SketchPoint& middle = arc.points[0];
    const double radius = distance(middle, arc.points[1]);
    const double span = distance(middle, arc.points[2]);
    if (!(span > 1e-12) || !(radius > 0.0)) {
        return std::nullopt;
    }
    return SketchPoint{middle.u + (arc.points[2].u - middle.u) * radius / span,
                       middle.v + (arc.points[2].v - middle.v) * radius / span};
}

// The places that count as "a point" (ends of lines and arcs, single points, the points of a
// spline and the point on a circle) and those that count as "a middle" (the middle of a line, the
// centre of a circle or an arc).
void collect(const std::vector<SketchElement>& elements, std::vector<Candidate>& points,
             std::vector<Candidate>& middles) {
    for (const SketchElement& element : elements) {
        if (!core::sketchPointsUsable(element.kind, element.points)) {
            continue;
        }
        switch (element.kind) {
        case SketchElementKind::point:
        case SketchElementKind::spline:
            for (const SketchPoint& point : element.points) {
                points.push_back({point, element.id});
            }
            break;
        case SketchElementKind::line:
            points.push_back({element.points[0], element.id});
            points.push_back({element.points[1], element.id});
            middles.push_back({{(element.points[0].u + element.points[1].u) / 2.0,
                                (element.points[0].v + element.points[1].v) / 2.0},
                               element.id});
            break;
        case SketchElementKind::circle:
            middles.push_back({element.points[0], element.id});
            points.push_back({element.points[1], element.id});
            break;
        case SketchElementKind::arc:
            middles.push_back({element.points[0], element.id});
            points.push_back({element.points[1], element.id});
            if (const auto end = arcEnd(element)) {
                points.push_back({*end, element.id});
            }
            break;
        }
    }
}

// The nearest of the candidates within `reach`, or nullptr.
const Candidate* nearest(const std::vector<Candidate>& candidates, const SketchPoint& pointer,
                         double reach) {
    const Candidate* best = nullptr;
    double bestDistance = reach;
    for (const Candidate& candidate : candidates) {
        const double d = distance(candidate.at, pointer);
        if (d <= bestDistance) {
            best = &candidate;
            bestDistance = d;
        }
    }
    return best;
}

double onGrid(double value, double step) {
    return std::round(value / step) * step + 0.0; // + 0.0: no negative zero
}

} // namespace

std::optional<SketchPoint> planePoint(const PlaneRay& ray, const core::SketchPlane& plane) {
    const geo::Point3d& n = plane.normal();
    const double along = ray.direction.x * n.x + ray.direction.y * n.y + ray.direction.z * n.z;
    const double length =
        std::sqrt(ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y +
                  ray.direction.z * ray.direction.z);
    if (!(length > 0.0) || !std::isfinite(length) || std::abs(along) < 1e-9 * length) {
        return std::nullopt; // no direction, or parallel to the plane
    }
    const double height = plane.heightOf(ray.origin);
    const double t = -height / along; // in units of the direction vector
    if (!(t > 0.0) || !std::isfinite(t)) {
        return std::nullopt; // the plane lies behind the eye
    }
    return plane.toPlane({ray.origin.x + t * ray.direction.x, ray.origin.y + t * ray.direction.y,
                          ray.origin.z + t * ray.direction.z});
}

double sketchGridStep(double baseStep, double pixelsPerMillimetre, double minPixels) {
    if (!(baseStep > 0.0) || !(pixelsPerMillimetre > 0.0) || !std::isfinite(baseStep) ||
        !std::isfinite(pixelsPerMillimetre)) {
        return 0.0;
    }
    static constexpr double kFactors[] = {1.0, 2.0, 5.0};
    double decade = 1.0;
    for (int guard = 0; guard < 40; ++guard) {
        for (const double factor : kFactors) {
            const double step = baseStep * decade * factor;
            if (step * pixelsPerMillimetre >= minPixels) {
                return step;
            }
        }
        decade *= 10.0;
    }
    return baseStep * decade;
}

SketchSnap sketchSnap(const SketchPoint& pointer, const std::vector<SketchElement>& elements,
                      const SketchSnapSettings& settings) {
    SketchSnap result;
    result.position = pointer;
    if (!settings.enabled || !std::isfinite(pointer.u) || !std::isfinite(pointer.v)) {
        return result;
    }
    const double reach = settings.pixelsPerMillimetre > 0.0
                             ? settings.radiusPixels / settings.pixelsPerMillimetre
                             : 0.0;

    std::vector<Candidate> points;
    std::vector<Candidate> middles;
    collect(elements, points, middles);

    if (const Candidate* hit = nearest(points, pointer, reach)) {
        result.position = hit->at;
        result.kind = SketchSnapKind::point;
        result.element = hit->element;
        return result;
    }
    if (const Candidate* hit = nearest(middles, pointer, reach)) {
        result.position = hit->at;
        result.kind = SketchSnapKind::middle;
        result.element = hit->element;
        return result;
    }

    // Lines up with a point: same v (horizontal) or same u (vertical), the nearest of each.
    const Candidate* horizontal = nullptr;
    const Candidate* vertical = nullptr;
    double horizontalGap = reach;
    double verticalGap = reach;
    for (const std::vector<Candidate>* list : {&points, &middles}) {
        for (const Candidate& candidate : *list) {
            const double dv = std::abs(candidate.at.v - pointer.v);
            if (dv <= horizontalGap) {
                horizontal = &candidate;
                horizontalGap = dv;
            }
            const double du = std::abs(candidate.at.u - pointer.u);
            if (du <= verticalGap) {
                vertical = &candidate;
                verticalGap = du;
            }
        }
    }
    if (horizontal != nullptr || vertical != nullptr) {
        SketchPoint at = pointer;
        // A way that does not line up goes to the grid, so the point still lands on a nice number.
        if (settings.gridStep > 0.0) {
            at.u = onGrid(at.u, settings.gridStep);
            at.v = onGrid(at.v, settings.gridStep);
        }
        if (horizontal != nullptr) {
            at.v = horizontal->at.v;
        }
        if (vertical != nullptr) {
            at.u = vertical->at.u;
        }
        result.position = at;
        result.kind = SketchSnapKind::alignment;
        if (horizontal != nullptr) {
            result.guides.push_back({horizontal->at, at});
        }
        if (vertical != nullptr) {
            result.guides.push_back({vertical->at, at});
        }
        return result;
    }

    if (settings.gridStep > 0.0) {
        result.position = {onGrid(pointer.u, settings.gridStep),
                           onGrid(pointer.v, settings.gridStep)};
        result.kind = SketchSnapKind::grid;
    }
    return result;
}

} // namespace fission::interaction
