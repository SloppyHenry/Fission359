// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QVariant>

#include <memory>

namespace fission::services {

/// A setting of the program (not of a document): its name in the settings file, the value it
/// has when nothing was stored, and what counts as a usable value. A stored value that is not
/// usable (a theme "pink", a scale of 5000 %) is treated as not stored.
template <class T> struct Key {
    const char* name;
    T fallback;
    bool (*valid)(const T&);
};

namespace keys {

inline bool anyOf(const QString& value, std::initializer_list<const char*> allowed) {
    for (const char* word : allowed) {
        if (value == QLatin1String(word)) {
            return true;
        }
    }
    return false;
}

/// "system", "de" or "en".
inline const Key<QString> language{"language", QStringLiteral("system"), [](const QString& v) {
                                       return anyOf(v, {"system", "de", "en"});
                                   }};
/// "dark", "light" or "system".
inline const Key<QString> theme{"theme", QStringLiteral("dark"), [](const QString& v) {
                                    return anyOf(v, {"system", "light", "dark"});
                                }};
/// "#rrggbb"; empty = the program's own accent colour.
inline const Key<QString> accentColor{"accentColor", QString(), [](const QString& v) {
                                          static const QRegularExpression hex(
                                              QStringLiteral("^#[0-9a-fA-F]{6}$"));
                                          return v.isEmpty() || hex.match(v).hasMatch();
                                      }};
/// Percent, 80 to 200.
inline const Key<int> uiScale{"uiScale", 100, [](const int& v) { return v >= 80 && v <= 200; }};
inline const Key<QString> mouseStyle{
    "mouseStyle", QStringLiteral("fission"), [](const QString& v) {
        return anyOf(v, {"fission", "fusion", "tinkercad", "blender", "sketchup"});
    }};
/// "axes" (arrows along the axes) or "classic" (squares and a lifting arrow, like Tinkercad).
inline const Key<QString> handleStyle{"handleStyle", QStringLiteral("axes"), [](const QString& v) {
                                          return anyOf(v, {"axes", "classic"});
                                      }};
inline const Key<QString> printerId{"printerId", QStringLiteral("generic"),
                                    [](const QString& v) { return !v.isEmpty(); }};
/// Millimetres; 0 = automatic (finer when zoomed in).
inline const Key<double> snapStep{"snapStep", 0.0,
                                  [](const double& v) { return v >= 0.0 && v <= 1000.0; }};
inline const Key<bool> snapGridShown{"snapGridShown", true, [](const bool&) { return true; }};
inline const Key<bool> softShadows{"softShadows", true, [](const bool&) { return true; }};
/// "mm" or "in": the unit of new documents.
inline const Key<QString> units{"units", QStringLiteral("mm"),
                                [](const QString& v) { return anyOf(v, {"mm", "in"}); }};
inline const Key<int> decimals{"decimals", 2, [](const int& v) { return v >= 0 && v <= 4; }};
inline const Key<bool> autosave{"autosave", true, [](const bool&) { return true; }};
inline const Key<int> autosaveMinutes{"autosaveMinutes", 1,
                                      [](const int& v) { return v >= 1 && v <= 60; }};
/// The first-start assistant was shown (and finished or skipped).
inline const Key<bool> firstRunDone{"firstRunDone", false, [](const bool&) { return true; }};

} // namespace keys

/// What the person set up: language, look, mouse, printer, units. Kept in the user's settings
/// file; one place for the names and the defaults (docs: AP-1.11a).
class Settings {
public:
    /// `file` empty: the user's settings of the program. A path: that INI file (tests, and the
    /// `--settings-dir` option).
    explicit Settings(const QString& file = {});

    /// The stored value, or the fallback when nothing (usable) is stored.
    template <class T> T value(const Key<T>& key) const {
        const QVariant stored = settings_->value(QLatin1String(key.name));
        if (!stored.isValid() || !stored.canConvert<T>()) {
            return key.fallback;
        }
        const T converted = stored.value<T>();
        return key.valid(converted) ? converted : key.fallback;
    }

    /// Stores a usable value; false (and nothing stored) for an unusable one.
    template <class T> bool set(const Key<T>& key, const T& value) {
        if (!key.valid(value)) {
            return false;
        }
        settings_->setValue(QLatin1String(key.name), QVariant::fromValue(value));
        settings_->sync();
        return true;
    }

    /// Forgets everything stored: back to the defaults.
    void reset();

private:
    std::unique_ptr<QSettings> settings_;
};

} // namespace fission::services
