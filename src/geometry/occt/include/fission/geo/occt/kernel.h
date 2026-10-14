// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "fission/geo/kernel.h"

namespace fission::geo::occt {

/// The OpenCASCADE implementation of the geometry kernel interface.
std::unique_ptr<GeometryKernel> makeKernel();

} // namespace fission::geo::occt
