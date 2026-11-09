// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include "fission/geo/sketch_edge.h"
#include "fission/geo/solid.h"

namespace fission::geo::occt {

/// See `GeometryKernel::projectEdgeToSketch`. Throws GeometryError.
SketchEdgeShape projectEdgeToSketch(const Solid& solid, std::uint32_t edgeId,
                                    const SketchFrame& frame);

} // namespace fission::geo::occt
