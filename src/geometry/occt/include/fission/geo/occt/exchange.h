// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/exchange.h"
#include "fission/geo/outline.h"

namespace fission::geo::occt {

/// See `GeometryKernel::importFile`.
ImportResult importFile(ImportFormat format, const std::vector<std::uint8_t>& bytes,
                        const ImportOptions& options);

/// See `GeometryKernel::exportStep`.
std::string exportStep(const std::vector<ExportBody>& bodies);

/// See `GeometryKernel::faceOutline`.
Outline faceOutline(const Solid& solid, std::uint32_t faceId);

} // namespace fission::geo::occt
