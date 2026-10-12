// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {

void collect(void* context, void* data, int size) {
    auto* bytes = static_cast<std::vector<unsigned char>*>(context);
    const auto* begin = static_cast<const unsigned char*>(data);
    bytes->insert(bytes->end(), begin, begin + size);
}

} // namespace

TEST_CASE("stb writes and reads back a PNG", "[smoke][stb]") {
    // 2x2 pixels, RGB: red, green, blue, white.
    const unsigned char pixels[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};

    std::vector<unsigned char> png;
    REQUIRE(stbi_write_png_to_func(collect, &png, 2, 2, 3, pixels, 6) != 0);
    REQUIRE_FALSE(png.empty());

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* decoded = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &width,
                                                   &height, &channels, 3);
    REQUIRE(decoded != nullptr);
    CHECK(width == 2);
    CHECK(height == 2);
    CHECK(decoded[0] == 255); // first pixel is red
    CHECK(decoded[1] == 0);
    stbi_image_free(decoded);
}
