// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace fission::ui {

/// Number input and output for the number fields (QML singleton "Numbers"): formulas, units
/// and both decimal separators in, the user's decimal separator out.
class Numbers : public QObject {
    Q_OBJECT

public:
    explicit Numbers(QObject* parent = nullptr) : QObject(parent) {}

    /// Value of what was typed in; kind is "length" (result in mm), "angle" or "plain".
    /// Returns NaN when the text is not a valid expression.
    Q_INVOKABLE double evaluate(const QString& text,
                                const QString& kind = QStringLiteral("length")) const;

    /// A number as text with at most `decimals` decimals and no trailing zeros ("12,5" in German,
    /// "12.5" in English).
    Q_INVOKABLE QString format(double value, int decimals = 3) const;
};

} // namespace fission::ui
