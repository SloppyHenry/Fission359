// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/extrude.h"
#include "fission/geo/named_solid.h"
#include "fission/geo/sketch_edge.h"
#include "fission/geo/sketch_regions.h"

namespace fission::geo::occt {

/// See `GeometryKernel::sketchRegions`. Throws GeometryError.
SketchRegionResult sketchRegions(const std::vector<SketchCurve>& curves);

/// See `GeometryKernel::extrudeRegion`. Throws GeometryError.
NamedSolid extrudeRegion(const std::vector<SketchCurve>& curves, Point2d inside,
                         const SketchFrame& frame, const ExtrudeSpec& spec);

} // namespace fission::geo::occt
