// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include "fission/geo/types.h"

namespace fission::geo {

/// The cut face of a body along a plane (AP-2.11a): the exact section as triangles. Holes stay
/// holes. Empty where the plane misses the body or only touches an outer face.
struct SectionMesh {
    std::vector<Point3d> positions;
    std::vector<std::uint32_t> indices; ///< Three per triangle, counter-clockwise seen against
                                        ///< the plane normal (the normal points at the viewer).
    double area = 0.0;                  ///< Exact area of the section (mm²), not of the triangles.

    bool empty() const noexcept { return indices.empty(); }
};

} // namespace fission::geo
