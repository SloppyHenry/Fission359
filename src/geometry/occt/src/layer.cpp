// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/layer.h"

#include <Standard_Version.hxx>

namespace fission::geo::occt {

std::string_view layerName() noexcept {
    return "fission_geometry_occt";
}

std::string_view kernelVersion() noexcept {
    return OCC_VERSION_STRING_EXT;
}

} // namespace fission::geo::occt
