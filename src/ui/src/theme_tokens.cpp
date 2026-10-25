// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/theme_tokens.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace fission::ui {

namespace {

using Rgb = std::array<double, 3>; // 0..255

Rgb toRgb(const QColor& c) {
    return {static_cast<double>(c.red()), static_cast<double>(c.green()),
            static_cast<double>(c.blue())};
}

QColor toColor(const Rgb& c) {
    const auto clamp8 = [](double v) {
        return static_cast<int>(std::lround(std::clamp(v, 0.0, 255.0)));
    };
    return QColor(clamp8(c[0]), clamp8(c[1]), clamp8(c[2]));
}

double luminance(const Rgb& c) {
    const auto channel = [](double v) {
        v /= 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c[0]) + 0.7152 * channel(c[1]) + 0.0722 * channel(c[2]);
}

double contrast(const Rgb& a, const Rgb& b) {
    const double x = luminance(a);
    const double y = luminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

struct Hsl {
    double h; // 0..360
    double s; // 0..1
    double l; // 0..1
};

Hsl toHsl(const Rgb& rgb) {
    const double r = rgb[0] / 255.0;
    const double g = rgb[1] / 255.0;
    const double b = rgb[2] / 255.0;
    const double mx = std::max({r, g, b});
    const double mn = std::min({r, g, b});
    const double l = (mx + mn) / 2.0;
    double h = 0.0;
    double s = 0.0;
    if (mx != mn) {
        const double d = mx - mn;
        s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
        if (mx == r) {
            h = (g - b) / d + (g < b ? 6.0 : 0.0);
        } else if (mx == g) {
            h = (b - r) / d + 2.0;
        } else {
            h = (r - g) / d + 4.0;
        }
        h *= 60.0;
    }
    return {h, s, l};
}

Rgb toRgb(const Hsl& hsl) {
    const auto k = [&hsl](double n) { return std::fmod(n + hsl.h / 30.0, 12.0); };
    const double a = hsl.s * std::min(hsl.l, 1.0 - hsl.l);
    const auto f = [&](double n) {
        return hsl.l - a * std::max(-1.0, std::min({k(n) - 3.0, 9.0 - k(n), 1.0}));
    };
    return {f(0) * 255.0, f(8) * 255.0, f(4) * 255.0};
}

Rgb mixRgb(const Rgb& a, const Rgb& b, double t) {
    return {a[0] * (1 - t) + b[0] * t, a[1] * (1 - t) + b[1] * t, a[2] * (1 - t) + b[2] * t};
}

// Moves the lightness step by step until the colour has `minimum` contrast on `background`.
Rgb shiftUntil(Hsl hsl, const Rgb& background, double direction, double minimum = 4.6) {
    for (int i = 0; i < 100 && contrast(toRgb(hsl), background) < minimum; ++i) {
        hsl.l = std::clamp(hsl.l + direction * 0.01, 0.0, 1.0);
    }
    return toRgb(hsl);
}

QColor rgba(int r, int g, int b, double alpha) {
    return QColor(r, g, b, static_cast<int>(std::lround(alpha * 255.0)));
}

void applyAccent(ThemeTokens& t, bool dark, const QColor& accent) {
    const QColor defaultBlue = defaultAccent();
    if (accent == defaultBlue) {
        // The values of the design table (docs/08-gestaltung.md §2).
        t.accent = QColor(0x25, 0x63, 0xD9);
        t.accentFg = QColor(0xFF, 0xFF, 0xFF);
        t.accentText = dark ? QColor(0x7F, 0xB0, 0xFF) : QColor(0x1F, 0x57, 0xC2);
        t.accentLine = dark ? QColor(0x5B, 0x97, 0xFF) : QColor(0x25, 0x63, 0xD9);
        t.accentSoft = dark ? QColor(0x22, 0x31, 0x4F) : QColor(0xDC, 0xE8, 0xFF);
        t.accentHover = dark ? QColor(0x2F, 0x6F, 0xE6) : QColor(0x1E, 0x57, 0xC4);
        return;
    }
    const Rgb white{255, 255, 255};
    const Hsl base = toHsl(toRgb(accent));
    const Rgb panel = toRgb(t.panel);
    const Rgb soft = mixRgb(panel, toRgb(accent), dark ? 0.22 : 0.14);

    // Accent text has to be readable on every surface it appears on: use the worst one.
    std::array<Rgb, 5> surfaces{panel, toRgb(t.field), toRgb(t.chrome), toRgb(t.view), soft};
    std::sort(surfaces.begin(), surfaces.end(),
              [](const Rgb& x, const Rgb& y) { return luminance(x) < luminance(y); });
    const Rgb& worst = dark ? surfaces.back() : surfaces.front();

    const Rgb button = shiftUntil(base, white, -1.0);
    const Rgb text =
        dark ? shiftUntil({base.h, std::min(base.s, 0.9), std::max(base.l, 0.6)}, worst, +1.0)
             : shiftUntil(base, worst, -1.0);
    const Rgb line = dark ? toRgb(Hsl{base.h, std::min(base.s, 0.95),
                                      std::clamp(std::max(base.l, 0.62), 0.0, 0.72)})
                          : button;
    // Hover is 5 % lighter (dark) or darker (light); in the dark design it may only go as far
    // as white text keeps 4.5 : 1, so that the hover state is readable, too.
    Hsl hover = toHsl(button);
    const double buttonLightness = hover.l;
    hover.l = std::clamp(hover.l + (dark ? 0.05 : -0.05), 0.0, 1.0);
    while (hover.l != buttonLightness && contrast(toRgb(hover), white) < 4.6) {
        hover.l += hover.l > buttonLightness ? -0.005 : 0.005;
        if (std::abs(hover.l - buttonLightness) < 0.005) {
            hover.l = buttonLightness;
        }
    }

    t.accent = toColor(button);
    t.accentFg = QColor(0xFF, 0xFF, 0xFF);
    t.accentText = toColor(text);
    t.accentLine = toColor(line);
    t.accentSoft = toColor(soft);
    t.accentHover = toColor(toRgb(hover));
}

} // namespace

QColor defaultAccent() {
    return QColor(0x25, 0x63, 0xD9);
}

ThemeTokens makeTokens(bool dark, const QColor& accent) {
    ThemeTokens t;
    if (dark) {
        t.chrome = QColor("#131416");
        t.bar = QColor("#1A1B1E");
        t.panel = QColor("#1F2023");
        t.field = QColor("#27292D");
        t.field2 = QColor("#2E3035");
        t.raise = QColor("#26282C");
        t.line = QColor("#34363B");
        t.line2 = QColor("#3A3D43");
        t.line3 = QColor("#4A4E55");
        t.view = QColor("#2B2D31");
        t.view2 = QColor("#232528");
        t.fg = QColor("#E8E9EB");
        t.fg2 = QColor("#C9CCD1");
        t.fg3 = QColor("#A4A8B0");
        t.fg4 = QColor("#979CA5");
        t.amber = QColor("#F0A53A");
        t.amberFg = QColor("#1A1205");
        t.amberSoft = rgba(240, 165, 58, 0.12);
        t.amberLine = rgba(240, 165, 58, 0.45);
        t.ok = QColor("#5DBB63");
        t.okSoft = rgba(93, 187, 99, 0.14);
        t.bad = QColor("#F47C74");
        t.badSoft = rgba(242, 100, 92, 0.14);
        t.bed = QColor("#25272B");
        t.bedEdge = QColor("#44474E");
        t.grid = QColor("#303237");
        t.gridMajor = QColor("#3A3D43");
        t.edge = rgba(10, 11, 13, 0.55);
        t.shadow = rgba(0, 0, 0, 0.6);
        t.shadowUi = rgba(0, 0, 0, 0.45);
        t.handle = QColor("#FFFFFF");
        t.handleLine = QColor("#16181B");
    } else {
        t.chrome = QColor("#E9EBEE");
        t.bar = QColor("#F0F1F3");
        t.panel = QColor("#F7F8F9");
        t.field = QColor("#FFFFFF");
        t.field2 = QColor("#F0F2F5");
        t.raise = QColor("#FFFFFF");
        t.line = QColor("#D9DCE1");
        t.line2 = QColor("#CDD1D7");
        t.line3 = QColor("#B5BBC4");
        t.view = QColor("#E3E6EA");
        t.view2 = QColor("#D4D8DE");
        t.fg = QColor("#1B1D21");
        t.fg2 = QColor("#3A3E45");
        t.fg3 = QColor("#50555E");
        t.fg4 = QColor("#5E646E");
        t.amber = QColor("#8F5600");
        t.amberFg = QColor("#FFFFFF");
        t.amberSoft = rgba(183, 110, 0, 0.10);
        t.amberLine = rgba(183, 110, 0, 0.45);
        t.ok = QColor("#17692F");
        t.okSoft = rgba(30, 125, 58, 0.10);
        t.bad = QColor("#B71F1F");
        t.badSoft = rgba(198, 40, 40, 0.08);
        t.bed = QColor("#CDD2D9");
        t.bedEdge = QColor("#A7AEB8");
        t.grid = QColor("#BCC2CB");
        t.gridMajor = QColor("#AAB1BC");
        t.edge = rgba(20, 24, 30, 0.45);
        t.shadow = rgba(30, 40, 60, 0.32);
        t.shadowUi = rgba(30, 40, 60, 0.18);
        t.handle = QColor("#FFFFFF");
        t.handleLine = QColor("#1C1F24");
    }
    t.axisX = QColor("#E5534B");
    t.axisY = QColor("#57AB5A");
    t.axisZ = QColor("#5B97FF");
    applyAccent(t, dark, accent.isValid() ? accent : defaultAccent());
    return t;
}

double contrastRatio(const QColor& a, const QColor& b) {
    return contrast(toRgb(a), toRgb(b));
}

QColor composite(const QColor& top, const QColor& base) {
    const double alpha = top.alphaF();
    return toColor(mixRgb(toRgb(base), toRgb(top), alpha));
}

QList<QPair<QString, QColor>> filamentPalette() {
    return {{QStringLiteral("black"), QColor("#2E2F33")},
            {QStringLiteral("white"), QColor("#ECE8E1")},
            {QStringLiteral("grey"), QColor("#8A8D93")},
            {QStringLiteral("red"), QColor("#D93F36")},
            {QStringLiteral("blue"), QColor("#3F83D6")},
            {QStringLiteral("green"), QColor("#3FA55B")},
            {QStringLiteral("yellow"), QColor("#F2B93A")},
            {QStringLiteral("orange"), QColor("#EE7B30")},
            {QStringLiteral("silver"), QColor("#C3C7CD")},
            {QStringLiteral("natural"), QColor("#D9C7A3")}};
}

QList<QPair<QString, QColor>> accentSuggestions() {
    return {{QStringLiteral("blue"), defaultAccent()},
            {QStringLiteral("violet"), QColor("#7C4DDB")},
            {QStringLiteral("teal"), QColor("#0E9AA7")},
            {QStringLiteral("pink"), QColor("#D6336C")},
            {QStringLiteral("slate"), QColor("#5A6B85")}};
}

QString accentClash(const QColor& accent) {
    const Hsl hsl = toHsl(toRgb(accent));
    if (hsl.s < 0.25) {
        return {};
    }
    if (hsl.h < 15.0 || hsl.h >= 345.0) {
        return QStringLiteral("fails");
    }
    if (hsl.h >= 25.0 && hsl.h < 65.0) {
        return QStringLiteral("needs help");
    }
    if (hsl.h >= 90.0 && hsl.h < 155.0) {
        return QStringLiteral("fits");
    }
    return {};
}

} // namespace fission::ui
