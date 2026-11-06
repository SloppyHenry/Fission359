// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/measuring.h"

namespace fission::geo::occt {

/// See `GeometryKernel::measureElements`.
Measurement measureElements(const std::vector<ElementRef>& elements);

} // namespace fission::geo::occt
