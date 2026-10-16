// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// How finely bodies are tessellated for the screen, depending on how far the camera is in
// (docs/02-ux-konzept.md §7.3: round stays round, however close you look).

#pragma once

#include "fission/geo/solid.h"

namespace fission::render {

/// 0 is the basic tessellation (chord error 0.01 mm, 5°); every level halves the chord error.
inline constexpr int kMaxDetailLevel = 4;

/// The tessellation of a level.
geo::TessellationParams tessellationForDetail(int level);

/// The level the view needs at `pixelsPerMillimetre` so that the mesh is off by at most about a
/// quarter of a pixel. `current` gives hysteresis: going back to a coarser level needs a
/// clearly smaller scale, so that the view does not flip at a threshold.
int detailLevelFor(float pixelsPerMillimetre, int current = 0);

} // namespace fission::render
