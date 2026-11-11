// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The regions of a sketch as the interface needs them (AP-2.1g): what the kernel found, with a
// stable id for each, and the pure helpers to find the region under the pointer. No window, no
// document. Regions are derived data: nothing of this is saved.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/geo/kernel.h"
#include "fission/geo/sketch_regions.h"

namespace fission::features {

struct SketchRegionInfo {
    /// Stable (decision 52): the sorted ids of the elements that bound the region (outer boundary
    /// and holes) joined by "+", e.g. "L1+L2+L3+L4". Moving an element does not change it; adding
    /// or removing a bounding element does. Regions that share the same elements (a lens and what
    /// is left of its two circles) get "/" and one letter for every element: `l` when the region
    /// is on its left (inside a circle), `r` on its right, `b` on both; if still equal, "#2", ...
    std::string id;
    std::vector<std::string> elements; ///< the bounding elements, sorted, no repeats
    geo::SketchRegion shape;
};

struct SketchRegionSet {
    std::vector<SketchRegionInfo> regions;
    std::size_t rejected = 0; ///< found by the kernel but failed the check: left out
};

/// The regions of the elements (helper lines and points are left out; the others are as the
/// kernel gets them). Throws `geo::GeometryError`.
SketchRegionSet computeSketchRegions(const geo::GeometryKernel& kernel,
                                     const std::vector<core::SketchElement>& elements);

/// True when `at` lies inside the region (inside the outer boundary and in none of the holes).
bool regionContains(const geo::SketchRegion& region, const core::SketchPoint& at);

/// The index of the smallest region that holds `at`; -1 when there is none.
int regionAt(const SketchRegionSet& set, const core::SketchPoint& at);

} // namespace fission::features
