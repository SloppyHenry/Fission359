// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// A scene with a given number of triangles for measuring the frame rate (AP-1.3: 60 fps at
// 2 million triangles). Not a model anyone would draw: undulating tiles on the print bed.

#pragma once

#include <memory>

#include "fission/render/scene.h"

namespace fission::render {

/// About `triangles` triangles in `bodies` tiles, laid out on the bed. The count is within a
/// few percent of the wish.
std::shared_ptr<const RenderScene> makeBenchmarkScene(std::size_t triangles, int bodies = 16);

} // namespace fission::render
