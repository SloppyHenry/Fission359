// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Private to src/geometry/occt: the face of one closed region of a sketch, shared by the region
// finder (sketch_regions.cpp) and the extrusion of a region (extrude.cpp). Names OCCT types.

#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "fission/geo/outline.h"
#include "fission/geo/sketch_curve.h"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>

namespace fission::geo::occt {

/// The face of the region of `curves` that contains `inside` (XY plane, z = 0), and for every
/// edge of it the indices of the curves it lies on. Empty optional when no region contains it.
struct RegionFace {
    TopoDS_Face face;
    std::vector<std::pair<TopoDS_Edge, std::vector<std::size_t>>> edgeCurves;
};

/// Throws GeometryError (kernelFailure) when the kernel cannot calculate.
std::optional<RegionFace> regionFaceAt(const std::vector<SketchCurve>& curves, Point2d inside);

} // namespace fission::geo::occt
