// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/exchange.h"

namespace fission::geo::occt {

/// See `GeometryKernel::importFile`.
ImportResult importFile(ImportFormat format, const std::vector<std::uint8_t>& bytes,
                        const ImportOptions& options);

/// See `GeometryKernel::exportStep`.
std::string exportStep(const std::vector<ExportBody>& bodies);

} // namespace fission::geo::occt
