// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace fission::services {
class Settings;
}

namespace fission::ui {

class Theme;
class Language;

/// The settings of the program as QML sees them (singleton "Preferences"): what the person
/// chose for language, look, mouse, printer and units. Every property reads from the settings
/// file and writes to it when set; language, theme and accent apply at once (rule: no restart).
/// Settings are no steps of the history and are not in the document file.
class Preferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    /// "#rrggbb", empty for the program's own blue.
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    /// Percent, 80 to 200.
    Q_PROPERTY(int uiScale READ uiScale WRITE setUiScale NOTIFY uiScaleChanged)
    Q_PROPERTY(QString mouseStyle READ mouseStyle WRITE setMouseStyle NOTIFY mouseStyleChanged)
    /// "axes" or "classic".
    Q_PROPERTY(QString handleStyle READ handleStyle WRITE setHandleStyle NOTIFY handleStyleChanged)
    Q_PROPERTY(QString printerId READ printerId WRITE setPrinterId NOTIFY printerIdChanged)
    Q_PROPERTY(double snapStep READ snapStep WRITE setSnapStep NOTIFY snapStepChanged)
    Q_PROPERTY(
        bool snapGridShown READ snapGridShown WRITE setSnapGridShown NOTIFY snapGridShownChanged)
    Q_PROPERTY(bool softShadows READ softShadows WRITE setSoftShadows NOTIFY softShadowsChanged)
    /// "mm" or "in": the unit of new documents.
    Q_PROPERTY(QString units READ units WRITE setUnits NOTIFY unitsChanged)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals NOTIFY decimalsChanged)
    Q_PROPERTY(bool autosave READ autosave WRITE setAutosave NOTIFY autosaveChanged)
    Q_PROPERTY(
        int autosaveMinutes READ autosaveMinutes WRITE setAutosaveMinutes NOTIFY autosaveChanged)
    Q_PROPERTY(bool firstRunDone READ firstRunDone WRITE setFirstRunDone NOTIFY firstRunDoneChanged)

public:
    /// `theme` and `language` are told at once when those settings change (either may be null in
    /// tests).
    Preferences(services::Settings* settings, Theme* theme, Language* language,
                QObject* parent = nullptr);

    QString language() const;
    QString theme() const;
    QString accentColor() const;
    int uiScale() const;
    QString mouseStyle() const;
    QString handleStyle() const;
    QString printerId() const;
    double snapStep() const;
    bool snapGridShown() const;
    bool softShadows() const;
    QString units() const;
    int decimals() const;
    bool autosave() const;
    int autosaveMinutes() const;
    bool firstRunDone() const;

    void setLanguage(const QString& value);
    void setTheme(const QString& value);
    void setAccentColor(const QString& value);
    void setUiScale(int value);
    void setMouseStyle(const QString& value);
    void setHandleStyle(const QString& value);
    void setPrinterId(const QString& value);
    void setSnapStep(double value);
    void setSnapGridShown(bool value);
    void setSoftShadows(bool value);
    void setUnits(const QString& value);
    void setDecimals(int value);
    void setAutosave(bool value);
    void setAutosaveMinutes(int value);
    void setFirstRunDone(bool value);

    /// Puts language, theme and accent from the settings into the running objects (at the start;
    /// the command line may have overridden some before: pass those in `skip...`).
    void applyToInterface(bool skipLanguage = false, bool skipTheme = false);

    /// Forgets everything: the defaults again (also applied).
    Q_INVOKABLE void resetAll();

    /// A warning word for an accent colour that is easily taken for a signal colour: "red",
    /// "yellow" or "green", otherwise empty (the interface turns it into a sentence).
    Q_INVOKABLE QString accentWarning(const QString& color) const;

signals:
    void languageChanged();
    void themeChanged();
    void accentColorChanged();
    void uiScaleChanged();
    void mouseStyleChanged();
    void handleStyleChanged();
    void printerIdChanged();
    void snapStepChanged();
    void snapGridShownChanged();
    void softShadowsChanged();
    void unitsChanged();
    void decimalsChanged();
    void autosaveChanged();
    void firstRunDoneChanged();

private:
    services::Settings* m_settings;
    Theme* m_theme;
    Language* m_language;
};

} // namespace fission::ui
