// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/numbers.h"

#include <cmath>
#include <limits>

#include <QLocale>

#include "fission/core/expression.h"

namespace fission::ui {

double Numbers::evaluate(const QString& text, const QString& kind) const {
    core::QuantityKind quantity = core::QuantityKind::length;
    if (kind == QStringLiteral("angle")) {
        quantity = core::QuantityKind::angle;
    } else if (kind == QStringLiteral("plain")) {
        quantity = core::QuantityKind::plain;
    }
    const std::optional<double> value = core::evaluateExpression(text.toStdString(), quantity);
    return value ? *value : std::numeric_limits<double>::quiet_NaN();
}

QString Numbers::format(double value, int decimals) const {
    if (!std::isfinite(value)) {
        return {};
    }
    QString text = QLocale().toString(value, 'f', decimals);
    const QString separator = QLocale().decimalPoint();
    if (text.contains(separator)) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(separator)) {
            text.chop(1);
        }
    }
    // The locale may group thousands; fields want the plain number.
    text.remove(QLocale().groupSeparator());
    if (text == QStringLiteral("-0")) {
        text = QStringLiteral("0");
    }
    return text;
}

} // namespace fission::ui
