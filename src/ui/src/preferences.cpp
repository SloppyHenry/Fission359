// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/preferences.h"

#include <QColor>

#include "fission/services/settings.h"
#include "fission/ui/language.h"
#include "fission/ui/theme.h"

namespace fission::ui {

namespace {
using namespace services;

// Writes `value` under `key` and tells whether it changed (and was usable).
template <class T> bool store(Settings* settings, const Key<T>& key, const T& value) {
    if (settings->value(key) == value) {
        return false;
    }
    return settings->set(key, value);
}
} // namespace

Preferences::Preferences(Settings* settings, Theme* theme, Language* language, QObject* parent)
    : QObject(parent), m_settings(settings), m_theme(theme), m_language(language) {}

QString Preferences::language() const {
    return m_settings->value(keys::language);
}
QString Preferences::theme() const {
    return m_settings->value(keys::theme);
}
QString Preferences::accentColor() const {
    return m_settings->value(keys::accentColor);
}
int Preferences::uiScale() const {
    return m_settings->value(keys::uiScale);
}
QString Preferences::mouseStyle() const {
    return m_settings->value(keys::mouseStyle);
}
QString Preferences::handleStyle() const {
    return m_settings->value(keys::handleStyle);
}
QString Preferences::printerId() const {
    return m_settings->value(keys::printerId);
}
double Preferences::snapStep() const {
    return m_settings->value(keys::snapStep);
}
bool Preferences::snapGridShown() const {
    return m_settings->value(keys::snapGridShown);
}
bool Preferences::softShadows() const {
    return m_settings->value(keys::softShadows);
}
QString Preferences::units() const {
    return m_settings->value(keys::units);
}
int Preferences::decimals() const {
    return m_settings->value(keys::decimals);
}
bool Preferences::autosave() const {
    return m_settings->value(keys::autosave);
}
int Preferences::autosaveMinutes() const {
    return m_settings->value(keys::autosaveMinutes);
}
bool Preferences::firstRunDone() const {
    return m_settings->value(keys::firstRunDone);
}

void Preferences::setLanguage(const QString& value) {
    if (store(m_settings, keys::language, value)) {
        if (m_language != nullptr) {
            m_language->setCode(value);
        }
        emit languageChanged();
    }
}

void Preferences::setTheme(const QString& value) {
    if (store(m_settings, keys::theme, value)) {
        if (m_theme != nullptr) {
            m_theme->setMode(value);
        }
        emit themeChanged();
    }
}

void Preferences::setAccentColor(const QString& value) {
    if (store(m_settings, keys::accentColor, value)) {
        if (m_theme != nullptr) {
            m_theme->setAccentBase(value.isEmpty() ? defaultAccent() : QColor(value));
        }
        emit accentColorChanged();
    }
}

void Preferences::setUiScale(int value) {
    if (store(m_settings, keys::uiScale, value)) {
        emit uiScaleChanged();
    }
}

void Preferences::setMouseStyle(const QString& value) {
    if (store(m_settings, keys::mouseStyle, value)) {
        emit mouseStyleChanged();
    }
}

void Preferences::setHandleStyle(const QString& value) {
    if (store(m_settings, keys::handleStyle, value)) {
        emit handleStyleChanged();
    }
}

void Preferences::setPrinterId(const QString& value) {
    if (store(m_settings, keys::printerId, value)) {
        emit printerIdChanged();
    }
}

void Preferences::setSnapStep(double value) {
    if (store(m_settings, keys::snapStep, value)) {
        emit snapStepChanged();
    }
}

void Preferences::setSnapGridShown(bool value) {
    if (store(m_settings, keys::snapGridShown, value)) {
        emit snapGridShownChanged();
    }
}

void Preferences::setSoftShadows(bool value) {
    if (store(m_settings, keys::softShadows, value)) {
        emit softShadowsChanged();
    }
}

void Preferences::setUnits(const QString& value) {
    if (store(m_settings, keys::units, value)) {
        emit unitsChanged();
    }
}

void Preferences::setDecimals(int value) {
    if (store(m_settings, keys::decimals, value)) {
        emit decimalsChanged();
    }
}

void Preferences::setAutosave(bool value) {
    if (store(m_settings, keys::autosave, value)) {
        emit autosaveChanged();
    }
}

void Preferences::setAutosaveMinutes(int value) {
    if (store(m_settings, keys::autosaveMinutes, value)) {
        emit autosaveChanged();
    }
}

void Preferences::setFirstRunDone(bool value) {
    if (store(m_settings, keys::firstRunDone, value)) {
        emit firstRunDoneChanged();
    }
}

void Preferences::applyToInterface(bool skipLanguage, bool skipTheme) {
    if (m_language != nullptr && !skipLanguage) {
        m_language->setCode(language());
    }
    if (m_theme != nullptr) {
        if (!skipTheme) {
            m_theme->setMode(theme());
        }
        const QString accent = accentColor();
        m_theme->setAccentBase(accent.isEmpty() ? defaultAccent() : QColor(accent));
    }
}

void Preferences::resetAll() {
    m_settings->reset();
    applyToInterface();
    emit languageChanged();
    emit themeChanged();
    emit accentColorChanged();
    emit uiScaleChanged();
    emit mouseStyleChanged();
    emit handleStyleChanged();
    emit printerIdChanged();
    emit snapStepChanged();
    emit snapGridShownChanged();
    emit softShadowsChanged();
    emit unitsChanged();
    emit decimalsChanged();
    emit autosaveChanged();
    emit firstRunDoneChanged();
}

QString Preferences::accentWarning(const QString& color) const {
    const QColor accent(color);
    if (!accent.isValid()) {
        return {};
    }
    // Signal colours carry meaning in the program (error red, warning yellow, "ok" green): an
    // accent that looks like one is easily taken for a message (R18). Judged by hue, with
    // enough colour in it (a grey is no signal).
    if (accent.hsvSaturationF() < 0.35 || accent.valueF() < 0.25) {
        return {};
    }
    const int hue = accent.hsvHue(); // 0 to 359
    if (hue < 0) {
        return {};
    }
    if (hue <= 15 || hue >= 340) {
        return QStringLiteral("red");
    }
    if (hue >= 40 && hue <= 70) {
        return QStringLiteral("yellow");
    }
    if (hue >= 90 && hue <= 150) {
        return QStringLiteral("green");
    }
    return {};
}

} // namespace fission::ui
