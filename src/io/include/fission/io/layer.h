// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace fission::io {

/// Name of this layer (CMake target name). Used in diagnostics and the layering tests.
std::string_view layerName() noexcept;

} // namespace fission::io
