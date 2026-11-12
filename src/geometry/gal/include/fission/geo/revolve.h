// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fission/geo/outline.h"

namespace fission::geo {

/// What turning a sketch region about an axis in the plane of the sketch asks for (AP-2.3c,
/// "Drehkörper"). The axis lies in the sketch plane and is given in sketch coordinates (u, v);
/// the region must lie entirely on one side of it (touching the axis is allowed).
struct RevolveSpec {
    /// A point of the axis, in sketch coordinates (u, v).
    Point2d axisPoint{0.0, 0.0};
    /// The direction of the axis, in sketch coordinates (u, v); not zero. A positive angle
    /// turns right-handed about this direction (the convention of `gp_Ax1`): looking from the
    /// tip of the direction back towards `axisPoint`, the area turns counter-clockwise. With
    /// the sketch normal n = u × v, a point on the +u side of an axis pointing along +v moves
    /// towards −n.
    Point2d axisDirection{0.0, 1.0};
    /// The angle of the turn in degrees, in (0, 360]. 360 makes the full body (no caps).
    double angleDegrees = 360.0;
};

} // namespace fission::geo
