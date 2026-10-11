// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Control for the layering test: uses only what the core layer offers. Must compile.

#include <string_view>

#include "fission/core/version.h"

std::string_view controlVersion() {
    return fission::core::versionString();
}
