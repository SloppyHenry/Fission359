// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <clocale>
#include <optional>
#include <string>

#include "fission/core/expression.h"
#include "fission/core/scaling.h"

using Catch::Approx;
using fission::core::evaluateExpression;
using fission::core::NamedMeasures;
using fission::core::numberText;
using fission::core::QuantityKind;
using fission::core::scaledExpression;

namespace {

std::string scaled(const char* expression, double factor) {
    const auto result = scaledExpression(expression, QuantityKind::length, factor);
    REQUIRE(result.has_value());
    return *result;
}

double valueOf(const std::string& expression, const NamedMeasures& names) {
    const auto value = evaluateExpression(expression, QuantityKind::length, names);
    REQUIRE(value.has_value());
    return *value;
}

} // namespace

TEST_CASE("scale: numbers are multiplied", "[scaling]") {
    CHECK(scaled("40", 1.5) == "60");
    CHECK(scaled("5", 2.0) == "10");
    CHECK(scaled("12.5", 0.5) == "6.25");
    CHECK(scaled("2,5", 2.0) == "5");
    CHECK(scaled("0", 3.0) == "0"); // a sharp corner stays sharp
    CHECK(scaled("20", 1.37) == "27.4");
    CHECK(scaled("10", 1.0 / 3.0) == "3.33333333333");
}

TEST_CASE("scale: a number with a unit or a sum without names becomes a number in mm",
          "[scaling]") {
    CHECK(scaled("2,5 cm", 2.0) == "50");
    CHECK(scaled("1 in", 1.0) == "25.4");
    CHECK(scaled("40/2", 1.5) == "30");
}

TEST_CASE("scale: a formula is kept, the factor is put behind it", "[scaling]") {
    const NamedMeasures names{{"Breite", 50.0}, {"Wand", 2.0}};
    CHECK(scaled("Breite/2", 1.5) == "(Breite/2) * 1.5");
    CHECK(scaled("Breite", 1.5) == "Breite * 1.5");
    CHECK(scaled("  Breite + 5 ", 2.0) == "(Breite + 5) * 2");
    CHECK(scaled("2 * Wand", 3.0) == "(2 * Wand) * 3");
    for (const char* formula : {"Breite/2", "Breite", "Breite + 5", "2 * Wand", "min(Breite; 30)",
                                "(Breite) + (Wand)", "Breite * 2 + 1"}) {
        INFO(formula);
        const std::string result = scaled(formula, 1.25);
        CHECK(valueOf(result, names) == Approx(1.25 * valueOf(formula, names)));
    }
}

TEST_CASE("scale: factors are put together, never piled up", "[scaling]") {
    CHECK(scaled("(Breite/2) * 1.5", 2.0) == "(Breite/2) * 3");
    CHECK(scaled("Breite * 1.5", 2.0) == "Breite * 3");
    CHECK(scaled("(Breite/2) * 1,5", 2.0) == "(Breite/2) * 3");
    // Back to where it was: the formula as typed.
    CHECK(scaled("(Breite/2) * 2", 0.5) == "Breite/2");
    CHECK(scaled("Breite * 1.5", 1.0 / 1.5) == "Breite");
    // Many steps, still one factor.
    std::string formula = "Breite/2";
    for (const double factor : {1.5, 2.0, 0.8, 1.1}) {
        formula = scaled(formula.c_str(), factor);
    }
    CHECK(formula == "(Breite/2) * 2.64");
}

TEST_CASE("scale: only a factor at the end of the whole formula is put together", "[scaling]") {
    // `* 2` here only belongs to b: not a factor of the whole.
    CHECK(scaled("a + b * 2", 1.5) == "(a + b * 2) * 1.5");
    CHECK(scaled("(a) + (b) * 2", 1.5) == "((a) + (b) * 2) * 1.5");
    CHECK(scaled("a * b * 2", 1.5) == "(a * b * 2) * 1.5");
    CHECK(scaled("(a + b) * c", 1.5) == "((a + b) * c) * 1.5");
    CHECK(scaled("(a + b) * 2 + 1", 1.5) == "((a + b) * 2 + 1) * 1.5");
}

TEST_CASE("scale: nonsense and impossible factors are refused", "[scaling]") {
    CHECK_FALSE(scaledExpression("5+", QuantityKind::length, 2.0).has_value());
    CHECK_FALSE(scaledExpression("", QuantityKind::length, 2.0).has_value());
    CHECK_FALSE(scaledExpression("5", QuantityKind::length, 0.0).has_value());
    CHECK_FALSE(scaledExpression("5", QuantityKind::length, -1.0).has_value());
    CHECK_FALSE(scaledExpression("5", QuantityKind::length, 1.0 / 0.0).has_value());
    CHECK_FALSE(scaledExpression("30°", QuantityKind::length, 2.0).has_value());
}

TEST_CASE("number text: short, with a dot, in every language", "[scaling]") {
    CHECK(numberText(12.5) == "12.5");
    CHECK(numberText(12.5000000000001) == "12.5");
    CHECK(numberText(-30.0) == "-30");
    CHECK(numberText(-1e-12) == "0");
    CHECK(numberText(1e13) == "10000000000000");
    CHECK(numberText(0.000012) == "0.000012");
    // A German computer writes "12,5" with printf: the values never do.
    const std::string before = std::setlocale(LC_NUMERIC, nullptr);
    if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") != nullptr) {
        CHECK(numberText(12.5) == "12.5");
        std::setlocale(LC_NUMERIC, before.c_str());
    }
}
