// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace fission::core {

/// Application version as "major.minor.patch", taken from the CMake project version.
std::string_view versionString() noexcept;

} // namespace fission::core
