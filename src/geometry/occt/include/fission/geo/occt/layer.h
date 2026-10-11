// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace fission::geo::occt {

/// Name of this layer (CMake target name). Used in diagnostics and the layering tests.
std::string_view layerName() noexcept;

} // namespace fission::geo::occt
