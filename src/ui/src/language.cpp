// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/language.h"

#include <QCoreApplication>
#include <QLocale>
#include <QQmlEngine>

namespace fission::ui {

Language::Language(QQmlEngine* engine, QObject* parent) : QObject(parent), m_engine(engine) {}

QStringList Language::available() {
    return {QStringLiteral("en"), QStringLiteral("de")};
}

void Language::setCode(const QString& requested) {
    QString code = requested;
    if (code == QStringLiteral("system")) {
        code = QLocale::system().language() == QLocale::German ? QStringLiteral("de")
                                                               : QStringLiteral("en");
    }
    if (!available().contains(code)) {
        return;
    }
    if (code == m_code && m_translator != nullptr) {
        return;
    }
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator.get());
        m_translator.reset();
    }
    // English is the language of the source texts; its file only adds the plural forms.
    {
        auto translator = std::make_unique<QTranslator>();
        if (translator->load(QStringLiteral(":/i18n/fission3d_%1.qm").arg(code))) {
            QCoreApplication::installTranslator(translator.get());
            m_translator = std::move(translator);
        } else {
            qWarning("Translation file for '%s' is missing", qPrintable(code));
        }
    }
    m_code = code;
    QLocale::setDefault(QLocale(code));
    if (m_engine != nullptr) {
        m_engine->retranslate();
    }
    emit codeChanged();
}

} // namespace fission::ui
