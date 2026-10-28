// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// How finely bodies are tessellated for the screen, depending on how far the camera is in
// (docs/02-ux-konzept.md §7.3: round stays round, however close you look).

#pragma once

#include "fission/geo/solid.h"

namespace fission::render {

/// 0 is the basic tessellation (chord error 0.01 mm, 5°); every level halves the chord error.
inline constexpr int kMaxDetailLevel = 4;

/// For a big body, the chord error of level 0 grows with its size: this share of the diagonal of
/// its box (0.01 mm at 100 mm). On screen that is still well under a pixel (the whole body
/// fits on the screen at most a few thousand pixels across), but a sphere of 800 mm no longer
/// needs hundreds of thousands of triangles: with the fixed 0.01 mm the program spent minutes on
/// its mesh and seemed to hang (feedback 2026-10-18).
inline constexpr double kRelativeChord = 1e-4;

/// The tessellation of a level, for a body whose box has the diagonal `size` (mm; 0 = small).
geo::TessellationParams tessellationForDetail(int level, double size = 0.0);

/// The level the view needs at `pixelsPerMillimetre` so that the mesh is off by at most about a
/// quarter of a pixel. `current` gives hysteresis: going back to a coarser level needs a
/// clearly smaller scale, so that the view does not flip at a threshold.
int detailLevelFor(float pixelsPerMillimetre, int current = 0);

} // namespace fission::render
