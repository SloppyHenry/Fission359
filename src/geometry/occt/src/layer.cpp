// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/layer.h"

#include <Standard_Version.hxx>
#include <TCollection_AsciiString.hxx>

namespace fission::geo::occt {

std::string_view layerName() noexcept {
    return "fission_geometry_occt";
}

std::string kernelVersion() {
    // A real call into the library (TKernel); the version macro alone would let the linker
    // drop the library and the "is OpenCASCADE loadable" check would prove nothing.
    const TCollection_AsciiString version(OCC_VERSION_STRING_EXT);
    return {version.ToCString(), static_cast<std::size_t>(version.Length())};
}

} // namespace fission::geo::occt
