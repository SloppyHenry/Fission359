// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/sketch_regions.h"

namespace fission::geo::occt {

/// See `GeometryKernel::sketchRegions`. Throws GeometryError.
SketchRegionResult sketchRegions(const std::vector<SketchCurve>& curves);

} // namespace fission::geo::occt
