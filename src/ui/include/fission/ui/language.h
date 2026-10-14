// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>

#include <memory>

class QQmlEngine;

namespace fission::ui {

/// The interface language. Texts are written in English in the source (`qsTr`); the German
/// translation is a Qt Linguist file. Switching retranslates the running interface.
class Language : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString code READ code WRITE setCode NOTIFY codeChanged)

public:
    explicit Language(QQmlEngine* engine, QObject* parent = nullptr);

    QString code() const { return m_code; }
    /// "de", "en" or "system" (the system language, German if it is German, else English).
    void setCode(const QString& code);

    /// The language codes the program ships.
    static QStringList available();

signals:
    void codeChanged();

private:
    QQmlEngine* m_engine;
    QString m_code{QStringLiteral("en")};
    std::unique_ptr<QTranslator> m_translator;
};

} // namespace fission::ui
