// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace fission::geo {

/// How far a sketch region is pulled out of its plane (AP-2.3a).
enum class ExtrudeExtent {
    distance,  ///< from the plane `distance` along the normal (negative: the other way)
    symmetric, ///< `distance` in total, half on each side of the plane
};

/// What a pull of a sketch region out of its plane asks for (AP-2.3a, "Ziehen").
struct ExtrudeSpec {
    double distance = 10.0; ///< mm, not zero
    ExtrudeExtent extent = ExtrudeExtent::distance;
};

} // namespace fission::geo
