// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/numbers.h"

#include <QLocale>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/expression.h"
#include "fission/core/scaling.h"

namespace fission::ui {

namespace {

core::QuantityKind quantityOf(const QString& kind) {
    if (kind == QStringLiteral("angle")) {
        return core::QuantityKind::angle;
    }
    if (kind == QStringLiteral("plain")) {
        return core::QuantityKind::plain;
    }
    return core::QuantityKind::length;
}

/// Letters that have to change to turn `a` into `b` (insert, delete, replace one).
int editDistance(const std::string& a, const std::string& b) {
    std::vector<int> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) {
        row[j] = static_cast<int>(j);
    }
    for (std::size_t i = 1; i <= a.size(); ++i) {
        int diagonal = row[0];
        row[0] = static_cast<int>(i);
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const int above = row[j];
            row[j] =
                std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

/// The candidate closest to `word` (case and umlauts folded), when it is at most two letters off.
std::string closest(const std::string& word, const std::vector<std::string>& candidates) {
    const std::string folded = core::foldMeasureName(word);
    std::string best;
    int bestDistance = 3;
    for (const std::string& candidate : candidates) {
        const int distance = editDistance(folded, core::foldMeasureName(candidate));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

const char* codeName(core::ExpressionErrorCode code) {
    using Code = core::ExpressionErrorCode;
    switch (code) {
    case Code::empty:
        return "empty";
    case Code::unexpectedCharacter:
        return "unexpectedCharacter";
    case Code::unexpectedEnd:
        return "unexpectedEnd";
    case Code::missingClosingBracket:
        return "missingClosingBracket";
    case Code::unknownName:
        return "unknownName";
    case Code::unknownFunction:
        return "unknownFunction";
    case Code::unknownUnit:
        return "unknownUnit";
    case Code::unitNotAllowed:
        return "unitNotAllowed";
    case Code::wrongArgumentCount:
        return "wrongArgumentCount";
    case Code::divisionByZero:
        return "divisionByZero";
    case Code::incompatibleQuantities:
        return "incompatibleQuantities";
    case Code::wrongKind:
        return "wrongKind";
    case Code::outOfRange:
        return "outOfRange";
    case Code::notFinite:
        return "notFinite";
    default:
        return "other";
    }
}

} // namespace

double Numbers::evaluate(const QString& text, const QString& kind) const {
    const core::QuantityKind quantity = quantityOf(kind);
    const std::optional<double> value =
        core::evaluateExpression(text.toStdString(), quantity, m_measures);
    return value ? *value : std::numeric_limits<double>::quiet_NaN();
}

QString Numbers::adjusted(const QString& text, const QString& kind, double delta) const {
    const core::QuantityKind quantity = quantityOf(kind);
    const std::optional<std::string> moved =
        core::adjustedExpression(text.toStdString(), quantity, delta);
    if (!moved) {
        return {};
    }
    // A number reads in the user's decimal separator; a formula keeps its dots.
    const auto parsed = core::Expression::parse(*moved);
    if (parsed && parsed.value().references().empty()) {
        if (const auto value = core::evaluateExpression(*moved, quantity)) {
            return format(*value, 9);
        }
    }
    return QString::fromStdString(*moved);
}

QVariantMap Numbers::explain(const QString& text, const QString& kind) const {
    const std::string source = text.toStdString();
    if (source.find_first_not_of(" \t") == std::string::npos) {
        return {};
    }
    auto outcome = core::Expression::parse(source);
    core::ExpressionError error;
    if (!outcome) {
        error = outcome.error();
    } else {
        std::map<std::string, double> folded;
        for (const auto& [name, value] : m_measures) {
            folded.emplace(core::foldMeasureName(name), value);
        }
        core::EvaluationContext context;
        context.kind = quantityOf(kind);
        context.lookup = [&folded](std::string_view key) -> std::optional<core::Quantity> {
            const auto found = folded.find(std::string(key));
            if (found == folded.end()) {
                return std::nullopt;
            }
            return core::Quantity{found->second, core::Dimension{1, 0}};
        };
        const auto result = outcome.value().evaluate(context);
        if (result) {
            return {};
        }
        error = result.error();
    }
    std::string suggestion;
    if (error.code == core::ExpressionErrorCode::unknownName) {
        std::vector<std::string> names;
        for (const auto& entry : m_measures) {
            names.push_back(entry.first);
        }
        suggestion = closest(error.subject, names);
    } else if (error.code == core::ExpressionErrorCode::unknownFunction) {
        suggestion = closest(error.subject, {"min", "max", "round", "sin", "cos", "sqrt"});
    } else if (error.code == core::ExpressionErrorCode::unknownUnit) {
        suggestion = closest(error.subject, {"mm", "cm", "m", "in", "ft"});
    }
    // The place is in bytes of the UTF-8 text; the field counts characters.
    const auto characters = [&source](std::size_t bytes) {
        return QString::fromUtf8(source.substr(0, std::min(bytes, source.size()))).size();
    };
    QVariantMap map;
    map.insert(QStringLiteral("code"), QString::fromLatin1(codeName(error.code)));
    map.insert(QStringLiteral("start"), characters(error.where.begin));
    map.insert(QStringLiteral("end"), characters(error.where.end));
    map.insert(QStringLiteral("subject"), QString::fromStdString(error.subject));
    map.insert(QStringLiteral("suggestion"), QString::fromStdString(suggestion));
    return map;
}

QString Numbers::format(double value, int decimals) const {
    if (!std::isfinite(value)) {
        return {};
    }
    QString text = QLocale().toString(value, 'f', std::min(decimals, m_maxDecimals));
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
