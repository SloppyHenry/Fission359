// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/sketch_curve.h"

namespace fission::geo::occt {

/// See `GeometryKernel::sketchSpline`. Throws GeometryError.
SketchSplineCurve sketchSpline(const std::vector<Point2d>& through, double tolerance);

} // namespace fission::geo::occt
