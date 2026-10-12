// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <string>
#include <string_view>

#include "fission/geo/occt/layer.h"

TEST_CASE("occt layer reports the OpenCASCADE version", "[occt][layer]") {
    const std::string version = fission::geo::occt::kernelVersion();

    REQUIRE_FALSE(version.empty());
    CHECK(std::isdigit(static_cast<unsigned char>(version.front())) != 0);
    CHECK(fission::geo::occt::layerName() == "fission_geometry_occt");
}
