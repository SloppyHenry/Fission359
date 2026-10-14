// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Theme singleton of the QML interface: colour tokens, fonts and the metrics of
// docs/08-gestaltung.md §3 and §4. QML never writes colour values or sizes by hand.

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

#include "fission/ui/theme_tokens.h"

namespace fission::ui {

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY tokensChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY tokensChanged)
    Q_PROPERTY(QColor accentBase READ accentBase WRITE setAccentBase NOTIFY tokensChanged)

#define FISSION_THEME_PROPERTY(name) \
    Q_PROPERTY(QColor name READ name##Color NOTIFY tokensChanged)
    FISSION_THEME_TOKENS(FISSION_THEME_PROPERTY)
#undef FISSION_THEME_PROPERTY

    Q_PROPERTY(QStringList fontFamilies READ fontFamilies CONSTANT)
    Q_PROPERTY(QStringList monoFamilies READ monoFamilies CONSTANT)

    // Sizes (px at 100 % scale)
    Q_PROPERTY(int radius MEMBER kRadius CONSTANT)
    Q_PROPERTY(int radiusFloat MEMBER kRadiusFloat CONSTANT)
    Q_PROPERTY(int radiusCard MEMBER kRadiusCard CONSTANT)
    Q_PROPERTY(int radiusDialog MEMBER kRadiusDialog CONSTANT)
    Q_PROPERTY(int controlHeight MEMBER kControlHeight CONSTANT)
    Q_PROPERTY(int smallControlHeight MEMBER kSmallControlHeight CONSTANT)
    Q_PROPERTY(int treeRowHeight MEMBER kTreeRowHeight CONSTANT)
    Q_PROPERTY(int menuBarHeight MEMBER kMenuBarHeight CONSTANT)
    Q_PROPERTY(int tabsHeight MEMBER kTabsHeight CONSTANT)
    Q_PROPERTY(int statusHeight MEMBER kStatusHeight CONSTANT)
    Q_PROPERTY(int leftPanelWidth MEMBER kLeftPanelWidth CONSTANT)
    Q_PROPERTY(int leftPanelWidthNarrow MEMBER kLeftPanelWidthNarrow CONSTANT)
    Q_PROPERTY(int rightPanelWidth MEMBER kRightPanelWidth CONSTANT)
    Q_PROPERTY(int rightPanelWidthNarrow MEMBER kRightPanelWidthNarrow CONSTANT)
    Q_PROPERTY(int narrowBreakpoint MEMBER kNarrowBreakpoint CONSTANT)
    Q_PROPERTY(int veryNarrowBreakpoint MEMBER kVeryNarrowBreakpoint CONSTANT)
    Q_PROPERTY(int duration MEMBER kDuration CONSTANT)
    Q_PROPERTY(int fontSize MEMBER kFontSize CONSTANT)
    Q_PROPERTY(int fontSizeSmall MEMBER kFontSizeSmall CONSTANT)
    Q_PROPERTY(int fontSizeSmallPlus MEMBER kFontSizeSmallPlus CONSTANT)
    Q_PROPERTY(int fontSizeTitle MEMBER kFontSizeTitle CONSTANT)
    Q_PROPERTY(int fontSizeLarge MEMBER kFontSizeLarge CONSTANT)

public:
    explicit Theme(QObject* parent = nullptr);

    QString mode() const { return m_mode; }
    /// "dark", "light" or "system".
    void setMode(const QString& mode);
    bool dark() const { return m_dark; }
    QColor accentBase() const { return m_accent; }
    void setAccentBase(const QColor& accent);

    /// The tokens in use, for C++ users (tests, the renderer palette).
    const ThemeTokens& tokens() const { return m_tokens; }

    static QStringList fontFamilies();
    static QStringList monoFamilies();

#define FISSION_THEME_GETTER(name) QColor name##Color() const { return m_tokens.name; }
    FISSION_THEME_TOKENS(FISSION_THEME_GETTER)
#undef FISSION_THEME_GETTER

signals:
    void tokensChanged();

private:
    void rebuild();

    static constexpr int kRadius = 6;
    static constexpr int kRadiusFloat = 9;
    static constexpr int kRadiusCard = 10;
    static constexpr int kRadiusDialog = 14;
    static constexpr int kControlHeight = 32;
    static constexpr int kSmallControlHeight = 28;
    static constexpr int kTreeRowHeight = 33;
    static constexpr int kMenuBarHeight = 40;
    static constexpr int kTabsHeight = 52;
    static constexpr int kStatusHeight = 28;
    static constexpr int kLeftPanelWidth = 248;
    static constexpr int kLeftPanelWidthNarrow = 216;
    static constexpr int kRightPanelWidth = 300;
    static constexpr int kRightPanelWidthNarrow = 264;
    static constexpr int kNarrowBreakpoint = 1100;
    static constexpr int kVeryNarrowBreakpoint = 760;
    static constexpr int kDuration = 180;
    // Font sizes are whole pixels (Qt Quick); the 12.5 and 14.5 px of the mockup are rounded.
    static constexpr int kFontSize = 13;
    static constexpr int kFontSizeSmall = 12;
    static constexpr int kFontSizeSmallPlus = 13;
    static constexpr int kFontSizeTitle = 15;
    static constexpr int kFontSizeLarge = 14;

    QString m_mode{QStringLiteral("dark")};
    bool m_dark = true;
    QColor m_accent;
    ThemeTokens m_tokens;
};

} // namespace fission::ui
