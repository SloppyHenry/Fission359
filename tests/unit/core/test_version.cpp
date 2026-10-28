// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <string_view>

#include "fission/core/layer.h"
#include "fission/core/version.h"

TEST_CASE("version string has the form major.minor.patch", "[core][version]") {
    const std::string_view version = fission::core::versionString();

    int dots = 0;
    for (const char c : version) {
        if (c == '.') {
            ++dots;
        } else {
            REQUIRE(std::isdigit(static_cast<unsigned char>(c)) != 0);
        }
    }
    CHECK(dots == 2);
}

TEST_CASE("core layer reports its name", "[core][layer]") {
    CHECK(fission::core::layerName() == "fission_core");
}
