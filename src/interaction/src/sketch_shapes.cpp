// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/sketch_shapes.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fission::interaction {

namespace {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kTinyAngle = 1e-9;

SketchElement made(SketchElementKind kind, std::vector<SketchPoint> points) {
    SketchElement element;
    element.kind = kind;
    element.points = std::move(points);
    return element;
}

double distance(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(b.u - a.u, b.v - a.v);
}

// An angle brought into [0, 2 pi).
double wrapped(double angle) {
    angle = std::fmod(angle, kTwoPi);
    return angle < 0.0 ? angle + kTwoPi : angle;
}

SketchElement arcOf(const SketchPoint& middle, const SketchPoint& start, const SketchPoint& end) {
    return made(SketchElementKind::arc, {middle, start, end});
}

} // namespace

std::optional<SketchElement> circleThroughThreePoints(const SketchPoint& a, const SketchPoint& b,
                                                      const SketchPoint& c) {
    const double abu = b.u - a.u;
    const double abv = b.v - a.v;
    const double acu = c.u - a.u;
    const double acv = c.v - a.v;
    const double cross = abu * acv - abv * acu;
    const double reach = std::max(abu * abu + abv * abv, acu * acu + acv * acv);
    // (nearly) on one line: the middle would be far away or not exist
    if (!(reach > 0.0) || std::abs(cross) <= 1e-9 * reach) {
        return std::nullopt;
    }
    const double ab2 = abu * abu + abv * abv;
    const double ac2 = acu * acu + acv * acv;
    const double du = (acv * ab2 - abv * ac2) / (2.0 * cross);
    const double dv = (abu * ac2 - acu * ab2) / (2.0 * cross);
    const SketchPoint middle{a.u + du, a.v + dv};
    if (!std::isfinite(middle.u) || !std::isfinite(middle.v) || distance(middle, a) > 1e7) {
        return std::nullopt;
    }
    return made(SketchElementKind::circle, {middle, a});
}

std::optional<SketchElement> arcThroughThreePoints(const SketchPoint& a, const SketchPoint& via,
                                                   const SketchPoint& c) {
    const auto circle = circleThroughThreePoints(a, via, c);
    if (!circle) {
        return std::nullopt;
    }
    const SketchPoint& middle = circle->points[0];
    const double cross = (via.u - a.u) * (c.v - a.v) - (via.v - a.v) * (c.u - a.u);
    return cross > 0.0 ? arcOf(middle, a, c) : arcOf(middle, c, a);
}

std::optional<SketchElement> arcOverChord(const SketchPoint& a, const SketchPoint& c, int side,
                                          double radius) {
    const double chord = distance(a, c);
    radius = std::abs(radius);
    if (!(chord > 0.0) || !std::isfinite(radius) || radius < chord / 2.0) {
        return std::nullopt;
    }
    const double half = chord / 2.0;
    const double height = std::sqrt(std::max(0.0, radius * radius - half * half));
    const double leftU = -(c.v - a.v) / chord;
    const double leftV = (c.u - a.u) / chord;
    const double sign = side > 0 ? 1.0 : -1.0;
    // the middle lies on the side away from the bulge
    const SketchPoint middle{(a.u + c.u) / 2.0 - sign * leftU * height,
                             (a.v + c.v) / 2.0 - sign * leftV * height};
    // bulging to the left of the walk a -> c means walking clockwise
    return sign > 0.0 ? arcOf(middle, c, a) : arcOf(middle, a, c);
}

std::optional<SketchElement> arcBySweep(const SketchPoint& middle, const SketchPoint& start,
                                        double sweep) {
    const double radius = distance(middle, start);
    if (!(radius >= kSketchMinimumSize) || !std::isfinite(sweep) || std::abs(sweep) < kTinyAngle ||
        std::abs(sweep) >= kTwoPi - kTinyAngle) {
        return std::nullopt;
    }
    const double from = std::atan2(start.v - middle.v, start.u - middle.u);
    const SketchPoint end{middle.u + radius * std::cos(from + sweep),
                          middle.v + radius * std::sin(from + sweep)};
    return sweep > 0.0 ? arcOf(middle, start, end) : arcOf(middle, end, start);
}

std::optional<TangentArc> tangentArc(const SketchPoint& from, const SketchPoint& direction,
                                     const SketchPoint& to, std::optional<double> radius) {
    const double leftU = -direction.v;
    const double leftV = direction.u;
    const double wu = to.u - from.u;
    const double wv = to.v - from.v;
    const double w2 = wu * wu + wv * wv;
    const double across = leftU * wu + leftV * wv; // how far `to` is left of the line
    if (!(w2 > 0.0) || std::abs(across) <= 1e-9 * std::sqrt(w2)) {
        return std::nullopt;
    }
    const bool clockwise = across < 0.0;
    const double sign = clockwise ? -1.0 : 1.0;
    double r = w2 / (2.0 * std::abs(across));
    if (radius) {
        r = std::abs(*radius);
    }
    if (!(r > 0.0) || !std::isfinite(r) || r > 1e7) {
        return std::nullopt;
    }
    const SketchPoint middle{from.u + sign * leftU * r, from.v + sign * leftV * r};
    SketchPoint end = to;
    if (radius) {
        const double reach = distance(middle, to);
        if (!(reach > 0.0)) {
            return std::nullopt;
        }
        end = {middle.u + (to.u - middle.u) * r / reach, middle.v + (to.v - middle.v) * r / reach};
    }
    const double a0 = std::atan2(from.v - middle.v, from.u - middle.u);
    const double a1 = std::atan2(end.v - middle.v, end.u - middle.u);
    const double sweep = clockwise ? wrapped(a0 - a1) : wrapped(a1 - a0);
    if (sweep < kTinyAngle || sweep > kTwoPi - kTinyAngle) {
        return std::nullopt;
    }
    TangentArc out;
    out.radius = r;
    out.clockwise = clockwise;
    out.end = end;
    out.arc = clockwise ? arcOf(middle, end, from) : arcOf(middle, from, end);
    const double radialU = (end.u - middle.u) / r;
    const double radialV = (end.v - middle.v) / r;
    out.endDirection = clockwise ? SketchPoint{radialV, -radialU} : SketchPoint{-radialV, radialU};
    return out;
}

SketchBatch slotElements(const SketchPoint& a, const SketchPoint& b, double width) {
    const double length = distance(a, b);
    if (!(length >= kSketchMinimumSize) || !(std::abs(width) >= kSketchMinimumSize) ||
        !std::isfinite(width)) {
        return {};
    }
    const double r = std::abs(width) / 2.0;
    const double offsetU = -(b.v - a.v) / length * r; // the left normal, as long as the radius
    const double offsetV = (b.u - a.u) / length * r;
    const SketchPoint aLeft{a.u + offsetU, a.v + offsetV};
    const SketchPoint aRight{a.u - offsetU, a.v - offsetV};
    const SketchPoint bLeft{b.u + offsetU, b.v + offsetV};
    const SketchPoint bRight{b.u - offsetU, b.v - offsetV};
    SketchBatch batch;
    batch.push_back(made(SketchElementKind::line, {aRight, bRight}));
    batch.push_back(arcOf(b, bRight, bLeft)); // the round end at b, counter-clockwise
    batch.push_back(made(SketchElementKind::line, {bLeft, aLeft}));
    batch.push_back(arcOf(a, aLeft, aRight)); // and the one at a
    return batch;
}

SketchBatch polygonLines(const SketchPoint& middle, double radius, int sides, double angle) {
    if (sides < kSketchMinimumSides || !(std::abs(radius) >= kSketchMinimumSize) ||
        !std::isfinite(radius) || !std::isfinite(angle)) {
        return {};
    }
    radius = std::abs(radius);
    std::vector<SketchPoint> corners;
    for (int i = 0; i < sides; ++i) {
        const double a = angle + kTwoPi * static_cast<double>(i) / static_cast<double>(sides);
        corners.push_back({middle.u + radius * std::cos(a), middle.v + radius * std::sin(a)});
    }
    SketchBatch batch;
    for (int i = 0; i < sides; ++i) {
        batch.push_back(
            made(SketchElementKind::line, {corners[static_cast<std::size_t>(i)],
                                           corners[static_cast<std::size_t>((i + 1) % sides)]}));
    }
    return batch;
}

} // namespace fission::interaction
