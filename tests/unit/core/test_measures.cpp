// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fission/core/measures.h"

using Catch::Approx;
using fission::core::ExpressionErrorCode;
using fission::core::MeasureDefinition;
using fission::core::QuantityKind;
using fission::core::resolveMeasures;

TEST_CASE("measures can use each other, in any order", "[measures]") {
    const auto resolution = resolveMeasures({
        {"Lochabstand", "Breite - 10 mm", QuantityKind::length},
        {"Breite", "50", QuantityKind::length},
        {"Hälfte", "Lochabstand / 2", QuantityKind::length},
    });
    REQUIRE(resolution.results.size() == 3);
    CHECK(resolution.find("Breite")->value->value == Approx(50.0));
    CHECK(resolution.find("lochabstand")->value->value == Approx(40.0));
    CHECK(resolution.find("HAELFTE")->value->value == Approx(20.0));

    // Dependencies come first in the calculation order.
    REQUIRE(resolution.order.size() == 3);
    CHECK(resolution.order[0] == 1);
    CHECK(resolution.order[1] == 0);
    CHECK(resolution.order[2] == 2);

    // "Verwendet in" of the measures table (UX concept §4).
    CHECK(resolution.find("Breite")->usedBy == std::vector<std::string>{"Lochabstand"});
    CHECK(resolution.find("Lochabstand")->dependsOn == std::vector<std::string>{"Breite"});
    CHECK(resolution.find("Hälfte")->usedBy.empty());
}

TEST_CASE("acceptance: a = b, b = a is a cycle with a readable path", "[measures][acceptance]") {
    const auto resolution = resolveMeasures({
        {"a", "b", QuantityKind::length},
        {"b", "a", QuantityKind::length},
    });
    for (const char* name : {"a", "b"}) {
        const auto* result = resolution.find(name);
        REQUIRE(result != nullptr);
        INFO(name);
        REQUIRE(result->error.has_value());
        CHECK_FALSE(result->value.has_value());
        CHECK(result->error->code == ExpressionErrorCode::cycle);
    }
    const auto& error = *resolution.find("a")->error;
    CHECK(error.path == std::vector<std::string>{"a", "b", "a"});
    // `where` is the "b" in a's formula.
    CHECK(error.where == fission::core::SourceRange{0, 1});
}

TEST_CASE("a measure that uses itself is a cycle", "[measures]") {
    const auto resolution = resolveMeasures({{"a", "a + 1", QuantityKind::length}});
    const auto& error = *resolution.results[0].error;
    CHECK(error.code == ExpressionErrorCode::cycle);
    CHECK(error.path == std::vector<std::string>{"a", "a"});
}

TEST_CASE("a cycle breaks only what depends on it", "[measures]") {
    const auto resolution = resolveMeasures({
        {"a", "b", QuantityKind::length},
        {"b", "c + 1", QuantityKind::length},
        {"c", "a", QuantityKind::length},
        {"d", "a * 2", QuantityKind::length},
        {"e", "5", QuantityKind::length},
        {"f", "e + 1", QuantityKind::length},
    });
    CHECK(resolution.find("a")->error->code == ExpressionErrorCode::cycle);
    CHECK(resolution.find("a")->error->path.size() == 4); // a, b, c, a
    const auto& d = *resolution.find("d")->error;
    CHECK(d.code == ExpressionErrorCode::brokenDependency);
    CHECK(d.subject == "a");
    CHECK(resolution.find("e")->value->value == Approx(5.0));
    CHECK(resolution.find("f")->value->value == Approx(6.0));
}

TEST_CASE("each mistake stays on its measure", "[measures]") {
    const auto resolution = resolveMeasures({
        {"Breite", "50", QuantityKind::length},
        {"breite", "60", QuantityKind::length}, // same name
        {"2x", "1", QuantityKind::length},
        {"Tiefe", "Breite + Hoehe", QuantityKind::length},
        {"Kaputt", "5 +", QuantityKind::length},
        {"Folge", "Kaputt * 2", QuantityKind::length},
        {"Winkel", "45°", QuantityKind::angle},
        {"Falsch", "Winkel", QuantityKind::length},
    });
    CHECK(resolution.results[0].value->value == Approx(50.0));
    CHECK(resolution.results[1].error->code == ExpressionErrorCode::duplicateName);
    CHECK(resolution.results[2].error->code == ExpressionErrorCode::invalidName);
    CHECK(resolution.results[3].error->code == ExpressionErrorCode::unknownName);
    CHECK(resolution.results[3].error->subject == "Hoehe");
    CHECK(resolution.results[4].error->code == ExpressionErrorCode::unexpectedEnd);
    CHECK(resolution.results[5].error->code == ExpressionErrorCode::brokenDependency);
    CHECK(resolution.results[5].error->subject == "Kaputt");
    CHECK(resolution.results[6].value->value == Approx(45.0));
    CHECK(resolution.results[7].error->code == ExpressionErrorCode::wrongKind);
}

TEST_CASE("the default unit applies to bare numbers in measures", "[measures]") {
    const auto resolution = resolveMeasures({{"Breite", "2", QuantityKind::length}}, 25.4);
    CHECK(resolution.results[0].value->value == Approx(50.8));
}

TEST_CASE("a long chain does not run out of stack", "[measures]") {
    std::vector<MeasureDefinition> chain;
    constexpr int count = 2000;
    chain.push_back({"m0", "1", QuantityKind::length});
    for (int i = 1; i < count; ++i) {
        chain.push_back(
            {"m" + std::to_string(i), "m" + std::to_string(i - 1) + " + 1", QuantityKind::length});
    }
    const auto resolution = resolveMeasures(chain);
    CHECK(resolution.find("m1999")->value->value == Approx(2000.0));
}
