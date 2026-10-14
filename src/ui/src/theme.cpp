// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/theme.h"

#include <QGuiApplication>
#include <QtGlobal>
#include <QStyleHints>

namespace fission::ui {

Theme::Theme(QObject* parent) : QObject(parent), m_accent(defaultAccent()) {
    rebuild();
    // "Like the system": follow the desktop when it switches between light and dark.
    if (auto* hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this] {
            if (m_mode == QStringLiteral("system")) {
                rebuild();
            }
        });
    }
}

void Theme::setMode(const QString& mode) {
    if (mode != QStringLiteral("dark") && mode != QStringLiteral("light") &&
        mode != QStringLiteral("system")) {
        return;
    }
    if (m_mode != mode) {
        m_mode = mode;
        rebuild();
    }
}

void Theme::setAccentBase(const QColor& accent) {
    if (accent.isValid() && accent != m_accent) {
        m_accent = accent;
        rebuild();
    }
}

void Theme::rebuild() {
    if (m_mode == QStringLiteral("system")) {
        const auto* hints = QGuiApplication::styleHints();
        // Dark is the default, also where the system does not tell.
        m_dark = hints == nullptr || hints->colorScheme() != Qt::ColorScheme::Light;
    } else {
        m_dark = m_mode == QStringLiteral("dark");
    }
    m_tokens = makeTokens(m_dark, m_accent);
    emit tokensChanged();
}

QStringList Theme::fontFamilies() {
    // IBM Plex Sans ships with the program. If it cannot be loaded, the system font takes over
    // (docs/08-gestaltung.md §3): San Francisco on macOS, Cantarell or Noto Sans on Linux.
#if defined(Q_OS_MACOS)
    return {QStringLiteral("IBM Plex Sans"), QStringLiteral(".AppleSystemUIFont"),
            QStringLiteral("Helvetica Neue")};
#else
    return {QStringLiteral("IBM Plex Sans"), QStringLiteral("Cantarell"),
            QStringLiteral("Noto Sans")};
#endif
}

QStringList Theme::monoFamilies() {
    return {QStringLiteral("IBM Plex Mono"), QStringLiteral("DejaVu Sans Mono"),
            QStringLiteral("Menlo")};
}

} // namespace fission::ui
