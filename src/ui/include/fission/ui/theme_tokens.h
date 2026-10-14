// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The colours of the interface (docs/08-gestaltung.md §2). No widget uses a colour value
// directly: everything reads these tokens through the Theme singleton.

#pragma once

#include <QColor>
#include <QList>
#include <QPair>
#include <QString>

namespace fission::ui {

// One entry per token: X(name, "css-like name used in tests and docs").
#define FISSION_THEME_TOKENS(X)                                                                    \
    X(chrome)                                                                                      \
    X(bar)                                                                                         \
    X(panel)                                                                                       \
    X(field)                                                                                       \
    X(field2)                                                                                      \
    X(raise)                                                                                       \
    X(line)                                                                                        \
    X(line2)                                                                                       \
    X(line3)                                                                                       \
    X(view)                                                                                        \
    X(view2)                                                                                       \
    X(fg)                                                                                          \
    X(fg2)                                                                                         \
    X(fg3)                                                                                         \
    X(fg4)                                                                                         \
    X(accent)                                                                                      \
    X(accentFg)                                                                                    \
    X(accentHover)                                                                                 \
    X(accentText)                                                                                  \
    X(accentLine)                                                                                  \
    X(accentSoft)                                                                                  \
    X(amber)                                                                                       \
    X(amberFg)                                                                                     \
    X(amberSoft)                                                                                   \
    X(amberLine)                                                                                   \
    X(ok)                                                                                          \
    X(okSoft)                                                                                      \
    X(bad)                                                                                         \
    X(badSoft)                                                                                     \
    X(axisX)                                                                                       \
    X(axisY)                                                                                       \
    X(axisZ)                                                                                       \
    X(bed)                                                                                         \
    X(bedEdge)                                                                                     \
    X(grid)                                                                                        \
    X(gridMajor)                                                                                   \
    X(edge)                                                                                        \
    X(shadow)                                                                                      \
    X(shadowUi)                                                                                    \
    X(handle)                                                                                      \
    X(handleLine)

struct ThemeTokens {
#define FISSION_TOKEN_MEMBER(name) QColor name;
    FISSION_THEME_TOKENS(FISSION_TOKEN_MEMBER)
#undef FISSION_TOKEN_MEMBER
};

/// The accent colour of the default theme (blue).
QColor defaultAccent();

/// All tokens for the dark or light design. The accent tokens are derived from `accent`
/// (docs/08-gestaltung.md §2, "Akzentfarbe"); the default blue gets the values of the table.
ThemeTokens makeTokens(bool dark, const QColor& accent = defaultAccent());

/// WCAG contrast ratio of two opaque colours (1 … 21).
double contrastRatio(const QColor& a, const QColor& b);

/// Colour `top` (may be translucent) drawn over `base`.
QColor composite(const QColor& top, const QColor& base);

/// Suggested accent colours (name key, colour), the first is the default.
QList<QPair<QString, QColor>> accentSuggestions();

/// Is the accent colour close to red, yellow or green (colours that mean something)?
/// Returns the meaning ("fails", "needs help", "fits") or an empty string.
QString accentClash(const QColor& accent);

} // namespace fission::ui
