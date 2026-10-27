// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "fission/core/expression.h"

namespace fission::ui {

/// Number input and output for the number fields (QML singleton "Numbers"): formulas, units
/// and both decimal separators in, the user's decimal separator out.
class Numbers : public QObject {
    Q_OBJECT

public:
    explicit Numbers(QObject* parent = nullptr) : QObject(parent) {}

    /// The named measures of the document that formulas may use (millimetres). Set by the
    /// document model whenever they change.
    void setNamedMeasures(core::NamedMeasures measures) { m_measures = std::move(measures); }

    /// Value of what was typed in; kind is "length" (result in mm), "angle" or "plain".
    /// Returns NaN when the text is not a valid expression.
    Q_INVOKABLE double evaluate(const QString& text,
                                const QString& kind = QStringLiteral("length")) const;

    /// A number as text with at most `decimals` decimals and no trailing zeros ("12,5" in German,
    /// "12.5" in English).
    /// The person's setting "decimals" (0 to 4) is a ceiling: nothing is shown with more.
    Q_INVOKABLE QString format(double value, int decimals = 3) const;
    void setMaxDecimals(int decimals) { m_maxDecimals = decimals; }

    /// The text moved by `delta` (millimetres or degrees) for the arrow keys: a number becomes the
    /// new number in the user's decimal separator, a formula stays one (`Breite/2` becomes
    /// `(Breite/2) + 1`). Empty when the text is not a valid expression.
    Q_INVOKABLE QString adjusted(const QString& text, const QString& kind, double delta) const;

    /// Why `text` is not a valid expression, for the line under the field: `code` (the name of
    /// the error, "unknownName", "missingClosingBracket", ...), `start` and `end` (the wrong part
    /// of the text, in characters), `subject` (the unknown word) and `suggestion` (the known
    /// name, function or unit that is closest to it, or empty). Empty map when the text is fine
    /// or empty.
    Q_INVOKABLE QVariantMap explain(const QString& text, const QString& kind) const;

private:
    core::NamedMeasures m_measures;
    int m_maxDecimals = 4;
};

} // namespace fission::ui
