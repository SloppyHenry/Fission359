// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fission/geo/types.h"

namespace fission::geo {

/// One edge of a body as a polyline (for the thin dark edge lines of the 3D view).
struct EdgePolyline {
    std::uint32_t edgeId = 0; ///< 1-based number of the edge in the body.
    std::vector<Point3f> points;
};

/// The display tessellation of one body (docs/03-architektur.md §5): triangles with a face number
/// each, plus the body's edges as polylines.
///
/// Every face has its own vertices, so hard edges stay hard and a face can be picked by its
/// number. Units are millimetres.
struct TriangleMesh {
    std::vector<Point3f> positions;
    std::vector<Point3f> normals; ///< Unit length, one per position, pointing outwards.
    std::vector<std::uint32_t>
        indices; ///< Three per triangle, counter-clockwise seen from outside.
    std::vector<std::uint32_t> faceIds; ///< One per triangle: 1-based face number in the body.
    std::vector<EdgePolyline> edges;

    std::size_t triangleCount() const noexcept { return indices.size() / 3; }
    bool empty() const noexcept { return indices.empty(); }
};

} // namespace fission::geo
