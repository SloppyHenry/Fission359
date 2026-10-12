// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <nanosvg.h>
#include <string>
#include <vector>

TEST_CASE("nanosvg parses a small SVG", "[smoke][nanosvg]") {
    std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"20\" height=\"10\">"
                      "<rect x=\"0\" y=\"0\" width=\"20\" height=\"10\"/></svg>";
    std::vector<char> buffer(svg.begin(), svg.end()); // nsvgParse modifies its input
    buffer.push_back('\0');

    NSVGimage* image = nsvgParse(buffer.data(), "px", 96.0f);
    REQUIRE(image != nullptr);
    CHECK(image->width == Catch::Approx(20.0f));
    CHECK(image->height == Catch::Approx(10.0f));
    CHECK(image->shapes != nullptr);
    nsvgDelete(image);
}
