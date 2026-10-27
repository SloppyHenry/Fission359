// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Aligning bodies (Ausrichten, AP-1.6e): the moves that put several bodies flush, or in the
// middle, along one axis. Pure geometry (mm), no window.

#pragma once

#include <vector>

#include "fission/interaction/distances.h"

namespace fission::interaction {

/// Where along the axis the bodies line up, with respect to the box round all of them.
enum class AlignTo { min, centre, max };

/// The move of each box (same order as `boxes`; only along `axis`, 0 = x, 1 = y, 2 = z) that
/// gives all of them the same lowest side (`min`), middle (`centre`) or highest side (`max`) on
/// that axis: the lowest side of the lowest box, the middle of the box round all of them, the
/// highest side of the highest box. A single box does not move.
std::vector<Vec3> alignOffsets(const std::vector<Box3>& boxes, int axis, AlignTo to);

} // namespace fission::interaction
