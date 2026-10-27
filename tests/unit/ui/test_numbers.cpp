// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QLocale>

#include <cmath>

#include "fission/ui/numbers.h"

using Catch::Approx;
using fission::ui::Numbers;

TEST_CASE("the number field evaluates what was typed", "[numbers]") {
    const Numbers numbers;
    CHECK(numbers.evaluate(QStringLiteral("2,5 cm")) == Approx(25.0));
    CHECK(numbers.evaluate(QStringLiteral("40/2")) == Approx(20.0));
    CHECK(numbers.evaluate(QStringLiteral("45°"), QStringLiteral("angle")) == Approx(45.0));
    CHECK(std::isnan(numbers.evaluate(QStringLiteral("abc"))));
}

TEST_CASE("numbers are written with the decimal separator of the language", "[numbers]") {
    const Numbers numbers;
    QLocale::setDefault(QLocale(QLocale::German));
    CHECK(numbers.format(12.5) == QStringLiteral("12,5"));
    CHECK(numbers.format(20.0) == QStringLiteral("20"));
    CHECK(numbers.format(1234.5) == QStringLiteral("1234,5"));
    QLocale::setDefault(QLocale(QLocale::English));
    CHECK(numbers.format(12.5) == QStringLiteral("12.5"));
    CHECK(numbers.format(-0.0) == QStringLiteral("0"));
    CHECK(numbers.format(0.30000000000000004) == QStringLiteral("0.3"));
}

TEST_CASE("a wrong name is explained with the closest known name", "[numbers][explain]") {
    Numbers numbers;
    numbers.setNamedMeasures({{"Breite", 40.0}, {"Höhe", 10.0}});

    const auto typo = numbers.explain(QStringLiteral("Breitee*2"), QStringLiteral("length"));
    CHECK(typo.value("code").toString() == QStringLiteral("unknownName"));
    CHECK(typo.value("subject").toString() == QStringLiteral("Breitee"));
    CHECK(typo.value("suggestion").toString() == QStringLiteral("Breite"));
    CHECK(typo.value("start").toInt() == 0);
    CHECK(typo.value("end").toInt() == 7);

    // The umlaut written out is still the same name.
    CHECK(numbers.explain(QStringLiteral("Hoehee"), QStringLiteral("length"))
              .value("suggestion")
              .toString() == QStringLiteral("Höhe"));
    // Three letters off is a different word: no guess.
    const auto far = numbers.explain(QStringLiteral("Tiefe"), QStringLiteral("length"));
    CHECK(far.value("code").toString() == QStringLiteral("unknownName"));
    CHECK(far.value("suggestion").toString().isEmpty());
    // The place counts characters, not bytes: "ö" is two bytes.
    CHECK(numbers.explain(QStringLiteral("Höhe + Tiefe"), QStringLiteral("length"))
              .value("start")
              .toInt() == 7);
}

TEST_CASE("other mistakes are named", "[numbers][explain]") {
    Numbers numbers;
    numbers.setNamedMeasures({{"Breite", 40.0}});
    const auto kindOf = [&numbers](const char* text, const char* kind = "length") {
        return numbers.explain(QString::fromUtf8(text), QString::fromUtf8(kind))
            .value("code")
            .toString();
    };
    CHECK(kindOf("(3") == QStringLiteral("missingClosingBracket"));
    CHECK(kindOf("5/0") == QStringLiteral("divisionByZero"));
    CHECK(kindOf("5 mm + 30°") == QStringLiteral("incompatibleQuantities"));
    CHECK(kindOf("30°", "length") == QStringLiteral("wrongKind"));
    CHECK(kindOf("2 mn") == QStringLiteral("unknownUnit"));
    CHECK(numbers.explain(QStringLiteral("2 mn"), QStringLiteral("length"))
              .value("suggestion")
              .toString() == QStringLiteral("mm"));
    CHECK(numbers.explain(QStringLiteral("rond(2)"), QStringLiteral("length"))
              .value("suggestion")
              .toString() == QStringLiteral("round"));
    // Fine or empty: nothing to explain.
    CHECK(numbers.explain(QStringLiteral("Breite/2"), QStringLiteral("length")).isEmpty());
    CHECK(numbers.explain(QStringLiteral("  "), QStringLiteral("length")).isEmpty());
}

TEST_CASE("the arrow keys move a number and keep a formula", "[numbers][arrows]") {
    const Numbers numbers;
    QLocale::setDefault(QLocale(QLocale::German));
    CHECK(numbers.adjusted(QStringLiteral("20"), QStringLiteral("length"), 0.1) ==
          QStringLiteral("20,1"));
    CHECK(numbers.adjusted(QStringLiteral("2,5 cm"), QStringLiteral("length"), 1.0) ==
          QStringLiteral("26"));
    CHECK(numbers.adjusted(QStringLiteral("Breite/2"), QStringLiteral("length"), 1.0) ==
          QStringLiteral("(Breite/2) + 1"));
    CHECK(numbers.adjusted(QStringLiteral("5+"), QStringLiteral("length"), 1.0).isEmpty());
    QLocale::setDefault(QLocale(QLocale::English));
}
