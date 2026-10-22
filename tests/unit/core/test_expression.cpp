// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <clocale>
#include <cstdint>
#include <string>

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

TEST_CASE("numbers read the same in a German environment", "[expression][locale]") {
    // Qt takes the C locale from the environment (LANG=de_DE.UTF-8 in a German terminal); a
    // number must not stop at the dot there.
    struct RestoreLocale {
        std::string saved = std::setlocale(LC_NUMERIC, nullptr);
        ~RestoreLocale() { std::setlocale(LC_NUMERIC, saved.c_str()); }
    } restore;
    const char* german = nullptr;
    for (const char* name : {"de_DE.UTF-8", "de_DE.utf8", "de_DE"}) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr) {
            german = name;
            break;
        }
    }
    if (german == nullptr) {
        SKIP("no German locale installed");
    }
    CHECK(mm("19.9") == Approx(19.9));
    CHECK(mm("19,9") == Approx(19.9));
    CHECK(mm("20 - 0.1") == Approx(19.9));
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

// ---------------------------------------------------------------------------------------------
// Expression: syntax tree, error places, functions, dimensions (AP-1.1a)

namespace {

using fission::core::EvaluationContext;
using fission::core::Expression;
using fission::core::ExpressionErrorCode;
using fission::core::Quantity;

fission::core::Outcome<Quantity> run(const char* text, QuantityKind kind = QuantityKind::length,
                                     double defaultUnit = 1.0) {
    const auto parsed = Expression::parse(text);
    if (!parsed) {
        return parsed.error();
    }
    EvaluationContext context;
    context.kind = kind;
    context.defaultUnitMillimetres = defaultUnit;
    context.lookup = [](std::string_view name) -> std::optional<Quantity> {
        if (name == "breite") {
            return Quantity{50.0, {1, 0}};
        }
        if (name == "hoehe") {
            return Quantity{20.0, {1, 0}};
        }
        if (name == "anzahl") {
            return Quantity{4.0, {0, 0}};
        }
        if (name == "winkel") {
            return Quantity{30.0, {0, 1}};
        }
        return std::nullopt;
    };
    return parsed.value().evaluate(context);
}

double value(const char* text, QuantityKind kind = QuantityKind::length) {
    const auto result = run(text, kind);
    INFO(text);
    REQUIRE(result.ok());
    return result.value().value;
}

fission::core::ExpressionError errorOf(const char* text, QuantityKind kind = QuantityKind::length) {
    const auto result = run(text, kind);
    INFO(text);
    REQUIRE_FALSE(result.ok());
    return result.error();
}

} // namespace

TEST_CASE("acceptance: 25,5 + 1 in is 50,9 mm", "[expression][acceptance]") {
    CHECK(value("25,5 + 1 in") == Approx(50.9));
    CHECK(value("25.5 + 1 in") == Approx(50.9));
}

TEST_CASE("functions: min, max, round, sin, cos, sqrt", "[expression]") {
    CHECK(value("min(5; 3; 9)") == Approx(3.0));
    CHECK(value("max(5; 3; 9)") == Approx(9.0));
    CHECK(value("min(2 cm; 15)") == Approx(15.0));
    CHECK(value("min(2, 5)") == Approx(2.0)); // comma and a space: two arguments
    CHECK(value("round(2,5)") == Approx(3.0));
    CHECK(value("round(-2,5)") == Approx(-3.0));
    CHECK(value("round(12,34; 0,5)") == Approx(12.5));
    CHECK(value("round(Breite / 3; 0,5)") == Approx(16.5));
    CHECK(value("sqrt(16)", QuantityKind::plain) == Approx(4.0));
    CHECK(value("sin(30°)", QuantityKind::plain) == Approx(0.5));
    CHECK(value("cos(60)", QuantityKind::plain) == Approx(0.5));
    CHECK(value("Hoehe * sin(Winkel)") == Approx(10.0));
    CHECK(value("MIN(1; 2)") == Approx(1.0));
}

TEST_CASE("right angles are exact", "[expression]") {
    CHECK(value("cos(90)", QuantityKind::plain) == 0.0);
    CHECK(value("sin(90)", QuantityKind::plain) == 1.0);
    CHECK(value("sin(180)", QuantityKind::plain) == 0.0);
    CHECK(value("cos(-90)", QuantityKind::plain) == 0.0);
    CHECK(value("sin(-90)", QuantityKind::plain) == -1.0);
    CHECK(value("sin(450)", QuantityKind::plain) == 1.0);
}

TEST_CASE("areas pass through formulas: the diagonal of a rectangle", "[expression]") {
    CHECK(value("sqrt(Breite*Breite + Hoehe*Hoehe)") == Approx(53.8516480713));
    CHECK(value("sqrt(3*3 + 4*4)", QuantityKind::plain) == Approx(5.0));
    // An area is not a length: the field says so instead of showing a wrong number.
    CHECK(errorOf("Breite * Hoehe").code == ExpressionErrorCode::wrongKind);
}

TEST_CASE("a bare number joins a length or an angle", "[expression]") {
    CHECK(value("Breite + 5") == Approx(55.0));
    CHECK(value("Breite / 2") == Approx(25.0));
    CHECK(value("Breite / Hoehe", QuantityKind::plain) == Approx(2.5));
    CHECK(value("Anzahl * 10") == Approx(40.0));
    CHECK(value("Winkel + 15", QuantityKind::angle) == Approx(45.0));
    CHECK(value("2 * 3 mm") == Approx(6.0));
    // A field that counts in inches: the bare number is inches, units still win.
    CHECK(run("2", QuantityKind::length, 25.4).value().value == Approx(50.8));
    CHECK(run("2 * 3", QuantityKind::length, 25.4).value().value == Approx(152.4));
}

TEST_CASE("units can sit behind brackets and names of plain numbers", "[expression]") {
    CHECK(value("(1 + 2) cm") == Approx(30.0));
    CHECK(value("Anzahl mm") == Approx(4.0));
    CHECK(value("1,5 m") == Approx(1500.0));
    CHECK(value("1 ft") == Approx(304.8));
    CHECK(value("90°", QuantityKind::angle) == Approx(90.0));
    CHECK(value("-(2 cm)") == Approx(-20.0));
}

TEST_CASE("errors say what and where", "[expression]") {
    SECTION("a name nobody knows") {
        const auto error = errorOf("Breite + Tiefee");
        CHECK(error.code == ExpressionErrorCode::unknownName);
        CHECK(error.subject == "Tiefee");
        CHECK(error.where == fission::core::SourceRange{9, 15});
    }
    SECTION("a unit nobody knows") {
        const auto error = errorOf("2 km");
        CHECK(error.code == ExpressionErrorCode::unknownUnit);
        CHECK(error.subject == "km");
        CHECK(error.where == fission::core::SourceRange{2, 4});
    }
    SECTION("two units") {
        const auto error = errorOf("2 cm mm");
        CHECK(error.code == ExpressionErrorCode::unitNotAllowed);
        CHECK(error.where == fission::core::SourceRange{5, 7});
    }
    SECTION("a unit after a length") {
        CHECK(errorOf("Breite cm").code == ExpressionErrorCode::unitNotAllowed);
    }
    SECTION("division by zero points at the divisor") {
        const auto error = errorOf("10 / (5 - 5)");
        CHECK(error.code == ExpressionErrorCode::divisionByZero);
        CHECK(error.where == fission::core::SourceRange{5, 12});
    }
    SECTION("text ends too early") {
        const auto error = errorOf("5 +");
        CHECK(error.code == ExpressionErrorCode::unexpectedEnd);
        CHECK(error.where.begin == 3);
    }
    SECTION("brackets") {
        CHECK(errorOf("(3 + 4").code == ExpressionErrorCode::missingClosingBracket);
        const auto extra = errorOf("3 + 4)");
        CHECK(extra.code == ExpressionErrorCode::unexpectedCharacter);
        CHECK(extra.where == fission::core::SourceRange{5, 6});
    }
    SECTION("functions") {
        CHECK(errorOf("wurzel(4)").code == ExpressionErrorCode::unknownFunction);
        const auto count = errorOf("min(2,5)"); // 2,5 is one number
        CHECK(count.code == ExpressionErrorCode::wrongArgumentCount);
        CHECK(count.subject == "min");
        CHECK(errorOf("sqrt(-4)", QuantityKind::plain).code == ExpressionErrorCode::outOfRange);
        CHECK(errorOf("round(5; 0)").code == ExpressionErrorCode::outOfRange);
    }
    SECTION("lengths and angles do not add up") {
        CHECK(errorOf("Breite + Winkel").code == ExpressionErrorCode::incompatibleQuantities);
        CHECK(errorOf("sin(Breite)", QuantityKind::plain).code ==
              ExpressionErrorCode::incompatibleQuantities);
    }
    SECTION("the field takes another kind") {
        CHECK(errorOf("2 cm", QuantityKind::plain).code == ExpressionErrorCode::wrongKind);
        CHECK(errorOf("Breite", QuantityKind::angle).code == ExpressionErrorCode::wrongKind);
    }
    SECTION("nothing at all") {
        CHECK(errorOf("   ").code == ExpressionErrorCode::empty);
    }
    SECTION("places count bytes, so umlauts are two") {
        const auto error = errorOf("Höhe2 + 1");
        CHECK(error.code == ExpressionErrorCode::unknownName);
        CHECK(error.where == fission::core::SourceRange{0, 6});
    }
}

TEST_CASE("an expression lists the measures it uses", "[expression]") {
    const auto parsed = Expression::parse("Breite/2 + min(Hoehe; 3) - breite");
    REQUIRE(parsed.ok());
    const auto& references = parsed.value().references();
    REQUIRE(references.size() == 3);
    CHECK(references[0].name == "Breite");
    CHECK(references[1].name == "Hoehe");
    CHECK(references[2].name == "breite");
    CHECK(references[1].where == fission::core::SourceRange{15, 20});
}

TEST_CASE("results too large to calculate with are errors", "[expression]") {
    CHECK(errorOf("1e5").code != ExpressionErrorCode::empty); // no exponent notation
    std::string huge(400, '9');
    const auto result = run(huge.c_str());
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ExpressionErrorCode::notFinite);
    std::string deep(500, '(');
    CHECK_FALSE(run((deep + "1" + std::string(500, ')')).c_str()).ok()); // no stack overflow
}

TEST_CASE("measure names", "[expression]") {
    using fission::core::isValidMeasureName;
    CHECK(isValidMeasureName("Breite"));
    CHECK(isValidMeasureName("Höhe_2"));
    CHECK(isValidMeasureName("_x"));
    CHECK_FALSE(isValidMeasureName(""));
    CHECK_FALSE(isValidMeasureName("2x"));
    CHECK_FALSE(isValidMeasureName("a b"));
    CHECK_FALSE(isValidMeasureName("a+b"));
}

TEST_CASE("random text never crashes and errors stay inside the text", "[expression][random]") {
    // Our own generator: the standard ones give other numbers on other systems.
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    const auto next = [&state] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    };
    const std::string alphabet = "0123456789.,;+-*/() \"°abmcinfhoesqrtlwkxyz_ÄÖ\xC3\x9F";
    for (int round = 0; round < 20000; ++round) {
        std::string text;
        const auto length = next() % 24;
        for (std::uint32_t i = 0; i < length; ++i) {
            text += alphabet[next() % alphabet.size()];
        }
        const auto result = run(text.c_str());
        if (!result.ok()) {
            INFO(text);
            CHECK(result.error().where.begin <= result.error().where.end);
            CHECK(result.error().where.end <= text.size());
        }
    }
}
