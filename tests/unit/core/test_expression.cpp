// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fission/core/expression.h"

using Catch::Approx;
using fission::core::evaluateExpression;
using fission::core::QuantityKind;

namespace {
double mm(const char* text) {
    const auto value = evaluateExpression(text);
    REQUIRE(value.has_value());
    return *value;
}
} // namespace

TEST_CASE("plain numbers, comma and dot are both decimal separators", "[expression]") {
    CHECK(mm("50") == Approx(50.0));
    CHECK(mm("2.5") == Approx(2.5));
    CHECK(mm("2,5") == Approx(2.5));
    CHECK(mm("  7  ") == Approx(7.0));
    CHECK(mm(".5") == Approx(0.5));
}

TEST_CASE("arithmetic follows the usual rules", "[expression]") {
    CHECK(mm("25+3") == Approx(28.0));
    CHECK(mm("40/2") == Approx(20.0));
    CHECK(mm("2+3*4") == Approx(14.0));
    CHECK(mm("(2+3)*4") == Approx(20.0));
    CHECK(mm("-5+8") == Approx(3.0));
    CHECK(mm("10-2-3") == Approx(5.0));
}

TEST_CASE("units convert to millimetres", "[expression]") {
    CHECK(mm("2,5 cm") == Approx(25.0));
    CHECK(mm("1 in") == Approx(25.4));
    CHECK(mm("1 Zoll") == Approx(25.4));
    CHECK(mm("1\"") == Approx(25.4));
    CHECK(mm("12mm") == Approx(12.0));
    CHECK(mm("1 cm + 5") == Approx(15.0));
}

TEST_CASE("named measures work in formulas, with or without umlauts", "[expression]") {
    const fission::core::NamedMeasures names{{"Breite", 50.0}, {"Höhe", 4.0}};
    CHECK(*evaluateExpression("Breite/2", QuantityKind::length, names) == Approx(25.0));
    CHECK(*evaluateExpression("breite + hoehe", QuantityKind::length, names) == Approx(54.0));
    CHECK(*evaluateExpression("HÖHE*2", QuantityKind::length, names) == Approx(8.0));
    CHECK_FALSE(evaluateExpression("Tiefe", QuantityKind::length, names).has_value());
}

TEST_CASE("angles and plain numbers take no length units", "[expression]") {
    CHECK(*evaluateExpression("45°", QuantityKind::angle) == Approx(45.0));
    CHECK(*evaluateExpression("90", QuantityKind::angle) == Approx(90.0));
    CHECK(*evaluateExpression("3", QuantityKind::plain) == Approx(3.0));
    CHECK_FALSE(evaluateExpression("2 cm", QuantityKind::plain).has_value());
}

TEST_CASE("a field can have a default unit other than millimetres", "[expression]") {
    CHECK(*evaluateExpression("2", QuantityKind::length, {}, 25.4) == Approx(50.8));
    CHECK(*evaluateExpression("2 mm", QuantityKind::length, {}, 25.4) == Approx(2.0));
}

TEST_CASE("nonsense is rejected instead of guessed", "[expression]") {
    for (const char* bad :
         {"", "abc", "5+", "(3", "3)", "1/0", "5 5", "1..2", "--", "*3", "2 km"}) {
        INFO(bad);
        CHECK_FALSE(evaluateExpression(bad).has_value());
    }
}
