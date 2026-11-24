// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "fission/core/expression.h"

namespace fission::ui {

/// Number input and output for the number fields (QML singleton "Numbers"): formulas, units
/// and both decimal separators in, the user's decimal separator out.
class Numbers : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY unitChanged)

public:
    explicit Numbers(QObject* parent = nullptr) : QObject(parent) {}

    /// The display unit for lengths, "mm" or "in"; any other word is ignored.
    QString unit() const;
    void setUnit(const QString& unit);

    /// The named measures of the document that formulas may use (millimetres). Set by the
    /// document model whenever they change.
    void setNamedMeasures(core::NamedMeasures measures) { m_measures = std::move(measures); }
    /// The names of all named measures, for the completion in the fields.
    void setMeasureNames(const QStringList& names) { m_measureNames = names; }
    /// The measures a number field offers when `typed` (the word at the cursor) was begun
    /// (AP-2.9): names that start with it first, then names that contain it, each group sorted;
    /// case and umlauts do not matter. Nothing for an empty word, and a name that is typed out
    /// completely is not offered again.
    Q_INVOKABLE QStringList completions(const QString& typed) const;

    /// Value of what was typed in; kind is "length" (result in mm), "angle" or "plain";
    /// a bare number counts in `unit`. Returns NaN when the text is not a valid expression.
    Q_INVOKABLE double evaluate(const QString& text,
                                const QString& kind = QStringLiteral("length")) const;

    /// A number as text with at most `decimals` decimals and no trailing zeros ("12,5" in German,
    /// "12.5" in English).
    /// The person's setting "decimals" (0 to 4) is a ceiling: nothing is shown with more.
    Q_INVOKABLE QString format(double value, int decimals = 3) const;
    void setMaxDecimals(int decimals) { m_maxDecimals = decimals; }

    /// The display unit for lengths as the symbol behind the numbers ("mm" or "in").
    Q_INVOKABLE QString unitSymbol() const;

    /// `millimetres` shown in `unit` (converted to inches when the unit is "in"), otherwise
    /// like `format`.
    Q_INVOKABLE QString formatLength(double millimetres, int decimals = 3) const;

    /// The text moved by `delta` for the arrow keys: a number becomes the new number in the
    /// user's decimal separator, a formula stays one (`Breite/2` becomes `(Breite/2) + 1`).
    /// For lengths `delta` is in the display unit (one step = 1 mm, or 1 in when the unit is
    /// "in"; a bare number then stays a number in inches). Empty when the text is not a valid
    /// expression.
    Q_INVOKABLE QString adjusted(const QString& text, const QString& kind, double delta) const;

    /// Why `text` is not a valid expression, for the line under the field: `code` (the name of
    /// the error, "unknownName", "missingClosingBracket", ...), `start` and `end` (the wrong part
    /// of the text, in characters), `subject` (the unknown word) and `suggestion` (the known
    /// name, function or unit that is closest to it, or empty). Empty map when the text is fine
    /// or empty.
    Q_INVOKABLE QVariantMap explain(const QString& text, const QString& kind) const;

signals:
    void unitChanged();

private:
    /// Millimetres in one display unit: 25.4 for "in", 1.0 for "mm".
    double millimetresPerUnit() const;

    core::NamedMeasures m_measures;
    QStringList m_measureNames;
    int m_maxDecimals = 4;
    QString m_unit = QStringLiteral("mm");
};

} // namespace fission::ui
