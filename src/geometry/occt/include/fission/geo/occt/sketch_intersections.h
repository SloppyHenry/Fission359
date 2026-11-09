// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/sketch_intersections.h"

namespace fission::geo::occt {

/// See `GeometryKernel::sketchIntersections` and `sketchCarrierCrossings`. Throw GeometryError.
std::vector<std::vector<SketchCrossing>>
sketchIntersections(const std::vector<SketchCurve>& curves);
std::vector<SketchCrossing> sketchCarrierCrossings(const SketchCurve& carrier,
                                                   const std::vector<SketchCurve>& others);

} // namespace fission::geo::occt
