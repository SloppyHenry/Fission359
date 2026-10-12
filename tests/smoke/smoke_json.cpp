// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

TEST_CASE("nlohmann-json parses and writes JSON", "[smoke][json]") {
    const auto document = nlohmann::json::parse(R"({"name":"Würfel","size":[10,20,30]})");

    CHECK(document["name"] == "Würfel");
    REQUIRE(document["size"].size() == 3);
    CHECK(document["size"][2] == 30);
    CHECK(nlohmann::json::parse(document.dump()) == document);
}
