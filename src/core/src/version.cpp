// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/version.h"

#ifndef FISSION_VERSION_STRING
#error "FISSION_VERSION_STRING must be defined by the build system"
#endif

namespace fission::core {

std::string_view versionString() noexcept {
    return FISSION_VERSION_STRING;
}

} // namespace fission::core
