// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Small plane maths shared by the sketch edits that are worked out exactly (offset, corners;
// AP-2.1e2). Private to the interaction layer. Points double as vectors.

#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "fission/core/sketch.h"

namespace fission::interaction::detail {

using Pt = core::SketchPoint;

inline constexpr double kTwoPi = 2.0 * std::numbers::pi;

inline Pt sub(const Pt& a, const Pt& b) {
    return {a.u - b.u, a.v - b.v};
}
inline Pt add(const Pt& a, const Pt& b) {
    return {a.u + b.u, a.v + b.v};
}
inline Pt mul(const Pt& a, double k) {
    return {a.u * k, a.v * k};
}
inline double dot(const Pt& a, const Pt& b) {
    return a.u * b.u + a.v * b.v;
}
inline double cross(const Pt& a, const Pt& b) {
    return a.u * b.v - a.v * b.u;
}
inline double norm(const Pt& a) {
    return std::hypot(a.u, a.v);
}
inline double dist(const Pt& a, const Pt& b) {
    return norm(sub(a, b));
}
inline Pt unit(const Pt& a) {
    const double n = norm(a);
    return n > 0.0 ? mul(a, 1.0 / n) : Pt{};
}
/// Turned a quarter to the left (counter-clockwise).
inline Pt left(const Pt& a) {
    return {-a.v, a.u};
}
inline double angleOf(const Pt& from, const Pt& to) {
    return std::atan2(to.v - from.v, to.u - from.u);
}
/// `a` brought into [0, 2π).
inline double wrapAngle(double a) {
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}
inline Pt onCircle(const Pt& centre, double radius, double angle) {
    return {centre.u + radius * std::cos(angle), centre.v + radius * std::sin(angle)};
}

/// A straight line (point and unit direction) or a circle (middle and radius).
struct Carrier {
    bool circle = false;
    Pt p;           ///< line: a point on it; circle: the middle
    Pt d;           ///< line: unit direction
    double r = 0.0; ///< circle: radius
};

/// All places where two carriers meet (up to two).
inline std::vector<Pt> carrierCrossings(const Carrier& a, const Carrier& b) {
    std::vector<Pt> out;
    if (!a.circle && !b.circle) {
        const double denominator = cross(a.d, b.d);
        if (std::abs(denominator) < 1e-12) {
            return out;
        }
        const double t = cross(sub(b.p, a.p), b.d) / denominator;
        out.push_back(add(a.p, mul(a.d, t)));
        return out;
    }
    if (a.circle && b.circle) {
        const double d = dist(a.p, b.p);
        if (d < 1e-12) {
            return out;
        }
        const double along = (a.r * a.r - b.r * b.r + d * d) / (2.0 * d);
        double h2 = a.r * a.r - along * along;
        if (h2 < -1e-9) {
            return out;
        }
        h2 = std::max(h2, 0.0);
        const Pt axis = mul(sub(b.p, a.p), 1.0 / d);
        const Pt base = add(a.p, mul(axis, along));
        const double h = std::sqrt(h2);
        out.push_back(add(base, mul(left(axis), h)));
        if (h > 1e-12) {
            out.push_back(sub(base, mul(left(axis), h)));
        }
        return out;
    }
    const Carrier& line = a.circle ? b : a;
    const Carrier& circle = a.circle ? a : b;
    const Pt w = sub(line.p, circle.p);
    const double half = dot(w, line.d);
    double disc = half * half - (dot(w, w) - circle.r * circle.r);
    if (disc < -1e-9) {
        return out;
    }
    disc = std::max(disc, 0.0);
    const double root = std::sqrt(disc);
    out.push_back(add(line.p, mul(line.d, -half + root)));
    if (root > 1e-12) {
        out.push_back(add(line.p, mul(line.d, -half - root)));
    }
    return out;
}

} // namespace fission::interaction::detail
