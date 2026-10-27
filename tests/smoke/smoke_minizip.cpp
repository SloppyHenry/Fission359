// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#include <string>
#include <vector>

TEST_CASE("minizip-ng writes and reads back a zip file", "[smoke][minizip]") {
    const auto path = (std::filesystem::temp_directory_path() / "fission359-smoke.zip").string();
    const std::string content = "document.json would be here";

    {
        void* writer = mz_zip_writer_create();
        REQUIRE(mz_zip_writer_open_file(writer, path.c_str(), 0, 0) == MZ_OK);

        mz_zip_file info;
        std::memset(&info, 0, sizeof(info));
        info.filename = "document.json";
        info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
        REQUIRE(mz_zip_writer_add_buffer(writer, const_cast<char*>(content.data()),
                                         static_cast<std::int32_t>(content.size()),
                                         &info) == MZ_OK);
        REQUIRE(mz_zip_writer_close(writer) == MZ_OK);
        mz_zip_writer_delete(&writer);
    }

    void* reader = mz_zip_reader_create();
    REQUIRE(mz_zip_reader_open_file(reader, path.c_str()) == MZ_OK);
    REQUIRE(mz_zip_reader_goto_first_entry(reader) == MZ_OK);
    REQUIRE(mz_zip_reader_entry_open(reader) == MZ_OK);

    std::vector<char> buffer(content.size() + 1, '\0');
    const std::int32_t read =
        mz_zip_reader_entry_read(reader, buffer.data(), static_cast<std::int32_t>(content.size()));
    CHECK(read == static_cast<std::int32_t>(content.size()));
    CHECK(std::string(buffer.data()) == content);

    mz_zip_reader_entry_close(reader);
    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    std::filesystem::remove(path);
}
