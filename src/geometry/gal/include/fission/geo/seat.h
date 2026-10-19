// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// How a shape is put down on a face (AP-1.6b3): the underside of the shape lies on the face, the
// shape stands on it at a right angle, and its upper edge points upward as far as the face
// allows. The result is plain numbers - three angles and the foot point - that are written into
// the step like any value the person could have typed (ADR-0010). The maths is plain (no
// kernel, no window).

#pragma once

#include <cmath>

#include "fission/geo/rotation.h"

namespace fission::geo {

/// The turn and the foot of a shape that sits on a face.
struct Seat {
    Angles turn;
    Point3d foot; ///< The `x, y, z` of the step (middle of the underside before the turn).
};

/// The turn that lays the underside (the shape's own +Z) on a face whose outward direction is
/// `normal` (need not be of unit length). The shape's own +Y points as far up as the face
/// allows and its +X to the right, seen from outside; a face looking straight up gives no turn,
/// one looking straight down turns the shape over around the X axis.
inline Matrix3 turnOntoFace(const Point3d& normal) noexcept {
    const double length =
        std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    const Point3d n = length > 0.0
                          ? Point3d{normal.x / length, normal.y / length, normal.z / length}
                          : Point3d{0.0, 0.0, 1.0};
    // "Up" along the face: the part of the world's up that lies in the face.
    Point3d up{-n.z * n.x, -n.z * n.y, 1.0 - n.z * n.z};
    double upLength = std::sqrt(up.x * up.x + up.y * up.y + up.z * up.z);
    if (upLength < 1e-9) {
        // The face looks straight up or down: there is no "up" along it. Keep X of the world.
        up = n.z > 0.0 ? Point3d{0.0, 1.0, 0.0} : Point3d{0.0, -1.0, 0.0};
        upLength = 1.0;
    }
    const Point3d y{up.x / upLength, up.y / upLength, up.z / upLength};
    const Point3d x{y.y * n.z - y.z * n.y, y.z * n.x - y.x * n.z, y.x * n.y - y.y * n.x};
    Matrix3 out;
    out.m = {{{x.x, y.x, n.x}, {x.y, y.y, n.y}, {x.z, y.z, n.z}}};
    return out;
}

/// Where a shape has to stand so that the middle of its underside touches `point` on a face with
/// the outward direction `normal`. `centreOffset` is the middle of the shape's box minus its
/// foot (before the turn): the shape is turned about that middle (ADR-0010). `hint` is the turn
/// the step has now, so that the angles do not jump to an equal turn.
inline Seat seatOnFace(const Point3d& point, const Point3d& normal, const Point3d& centreOffset,
                       const Angles& hint = {}) noexcept {
    Seat seat;
    seat.turn = anglesOf(turnOntoFace(normal), hint);
    // Tenths of a degree: a slope gives numbers a person can read and type (at 20 mm lever the
    // rounding moves a point by less than 0.04 mm). The foot is worked out from the rounded
    // angles, so the shape still touches the face exactly.
    for (double* angle : {&seat.turn.x, &seat.turn.y, &seat.turn.z}) {
        *angle = std::round(*angle * 10.0) / 10.0;
        if (*angle == 0.0) {
            *angle = 0.0; // no -0
        }
    }
    const Point3d turned = rotationOf(seat.turn).apply(centreOffset);
    seat.foot = {point.x - centreOffset.x + turned.x, point.y - centreOffset.y + turned.y,
                 point.z - centreOffset.z + turned.z};
    // Rounding noise of the sines is not a value a person should see in a number field.
    for (double* value : {&seat.foot.x, &seat.foot.y, &seat.foot.z}) {
        *value = std::round(*value * 1e3) / 1e3; // micrometres
        if (*value == 0.0) {
            *value = 0.0; // no -0
        }
    }
    return seat;
}

} // namespace fission::geo
