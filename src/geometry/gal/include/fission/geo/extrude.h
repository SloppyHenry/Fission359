// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include "fission/geo/solid.h"

namespace fission::geo {

/// How far a sketch region is pulled out of its plane (AP-2.3a).
enum class ExtrudeExtent {
    distance,  ///< from the plane `distance` along the normal (negative: the other way)
    symmetric, ///< `distance` in total, half on each side of the plane
    /// Far enough to pass completely through the body `through`; `distance` only gives the
    /// side: >= 0 along the normal, < 0 the other way.
    throughAll,
};

/// What a pull of a sketch region out of its plane asks for (AP-2.3a, "Ziehen").
struct ExtrudeSpec {
    double distance = 10.0; ///< mm, not zero
    ExtrudeExtent extent = ExtrudeExtent::distance;
    double taperDegrees =
        0.0; ///< sides lean inwards (> 0), < 89; outwards (< 0) is refused for now
    std::optional<Solid> through; ///< for throughAll
};

} // namespace fission::geo
