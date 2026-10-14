// SPDX-FileCopyrightText: Fission3D contributors
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
