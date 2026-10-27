// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// How a body is turned (AP-1.6b, ADR-0010): three angles, always in the same order. First around
// the X axis of the bed, then around Y, then around Z, all three axes fixed in space and meeting
// in the middle of the body. A turn of the body flat on the bed (around Z) is then always exactly
// one value. The maths is plain (no kernel, no window) so that the features, the kernel and the
// interface agree on it.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "fission/geo/types.h"

namespace fission::geo {

/// The turn of a body: degrees around the X, Y and Z axis of the bed, applied in that order.
struct Angles {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    bool isZero() const noexcept { return x == 0.0 && y == 0.0 && z == 0.0; }
    bool operator==(const Angles&) const = default;
};

/// A 3 × 3 rotation matrix, `m[row][column]`, acting on column vectors.
struct Matrix3 {
    std::array<std::array<double, 3>, 3> m{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

    Point3d apply(const Point3d& v) const noexcept {
        return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
    }

    /// First `other`, then this one.
    Matrix3 operator*(const Matrix3& other) const noexcept {
        Matrix3 out;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                out.m[r][c] =
                    m[r][0] * other.m[0][c] + m[r][1] * other.m[1][c] + m[r][2] * other.m[2][c];
            }
        }
        return out;
    }
};

inline constexpr double kDegrees = 180.0 / std::numbers::pi;

/// The turn by `degrees` around one axis of the bed (0 = X, 1 = Y, 2 = Z), counter-clockwise
/// seen from the tip of the axis.
inline Matrix3 axisRotation(int axis, double degrees) noexcept {
    const double radians = degrees / kDegrees;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    Matrix3 out;
    switch (axis) {
    case 0:
        out.m = {{{1, 0, 0}, {0, c, -s}, {0, s, c}}};
        break;
    case 1:
        out.m = {{{c, 0, s}, {0, 1, 0}, {-s, 0, c}}};
        break;
    default:
        out.m = {{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}};
        break;
    }
    return out;
}

/// The matrix of three angles: Z · Y · X (X acts first).
inline Matrix3 rotationOf(const Angles& angles) noexcept {
    return axisRotation(2, angles.z) * axisRotation(1, angles.y) * axisRotation(0, angles.x);
}

/// `point` turned by `angles` around `centre`.
inline Point3d rotatedAbout(const Point3d& point, const Point3d& centre,
                            const Angles& angles) noexcept {
    const Point3d turned =
        rotationOf(angles).apply({point.x - centre.x, point.y - centre.y, point.z - centre.z});
    return {centre.x + turned.x, centre.y + turned.y, centre.z + turned.z};
}

/// `vector` turned by `angles` (no centre: a direction).
inline Point3d rotatedDirection(const Point3d& vector, const Angles& angles) noexcept {
    return rotationOf(angles).apply(vector);
}

namespace detail {

inline double wrapped(double degrees) noexcept {
    return degrees - 360.0 * std::round(degrees / 360.0);
}

/// `value` plus a whole number of turns, as near to `hint` as possible.
inline double nearTo(double value, double hint) noexcept {
    return value + 360.0 * std::round((hint - value) / 360.0);
}

inline double distance(const Angles& a, const Angles& b) noexcept {
    return std::abs(wrapped(a.x - b.x)) + std::abs(wrapped(a.y - b.y)) +
           std::abs(wrapped(a.z - b.z));
}

} // namespace detail

/// The three angles of a turn. Many triples give the same turn (360° more or less, and a second
/// way round with Y on the other side of 90°); the one closest to `hint` is returned, so that a
/// number the person typed or a value that moves smoothly does not jump. Where the turn leaves
/// the first and last angle undecided (Y at ±90°), Z stays as in `hint`.
inline Angles anglesOf(const Matrix3& turn, const Angles& hint = {}) noexcept {
    const auto& m = turn.m;
    const double sinY = std::clamp(-m[2][0], -1.0, 1.0);
    Angles first;
    Angles second;
    if (std::abs(sinY) > 1.0 - 1e-12) {
        // Gimbal lock: X and Z turn about the same axis; only their sum or difference is known.
        first.y = sinY > 0 ? 90.0 : -90.0;
        first.z = hint.z;
        const Matrix3 rest = axisRotation(2, -hint.z) * turn;
        // rest = Ry(±90°) · Rx(x): its entries [1][1] and [1][2] are cos x and −sin x.
        first.x = std::atan2(-rest.m[1][2], rest.m[1][1]) * kDegrees;
        second = first;
    } else {
        const double y = std::asin(sinY) * kDegrees;
        const auto solve = [&](double yDegrees) {
            Angles out;
            const double cosY = std::cos(yDegrees / kDegrees);
            out.y = yDegrees;
            out.x = std::atan2(m[2][1] / cosY, m[2][2] / cosY) * kDegrees;
            out.z = std::atan2(m[1][0] / cosY, m[0][0] / cosY) * kDegrees;
            return out;
        };
        first = solve(y);
        second = solve(y >= 0 ? 180.0 - y : -180.0 - y);
    }
    Angles best = detail::distance(first, hint) <= detail::distance(second, hint) ? first : second;
    best.x = detail::nearTo(best.x, hint.x);
    best.y = detail::nearTo(best.y, hint.y);
    best.z = detail::nearTo(best.z, hint.z);
    // Turned back from -0 and from rounding noise: an angle that is zero is written as zero.
    for (double* angle : {&best.x, &best.y, &best.z}) {
        if (std::abs(*angle) < 1e-9) {
            *angle = 0.0;
        }
    }
    return best;
}

/// The angles of the turn that results from turning a body that already stands at `start` by
/// `degrees` around the axis `axis` of the bed (the way a ring of the turning tool works).
inline Angles turnedAbout(const Angles& start, int axis, double degrees,
                          const Angles& hint) noexcept {
    return anglesOf(axisRotation(axis, degrees) * rotationOf(start), hint);
}

} // namespace fission::geo
