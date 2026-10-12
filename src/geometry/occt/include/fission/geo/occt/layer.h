// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace fission::geo::occt {

/// Name of this layer (CMake target name). Used in diagnostics and the layering tests.
std::string_view layerName() noexcept;

/// Version of the linked OpenCASCADE library, for example "8.0.1".
/// Calls into the library, so it also proves that OpenCASCADE is found and loadable.
std::string kernelVersion();

} // namespace fission::geo::occt
