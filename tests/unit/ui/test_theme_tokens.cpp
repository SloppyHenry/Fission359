// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Contrast test of the design tokens (docs/08-gestaltung.md §2 and §8): every combination of
// text token and surface that appears in the interface reaches WCAG AA (4.5 : 1), in the dark
// and in the light design, also for derived accent colours.

#include <catch2/catch_test_macros.hpp>

#include <QString>

#include <vector>

#include "fission/ui/theme_tokens.h"

using namespace fission::ui;

namespace {

struct Surface {
    const char* name;
    QColor color;
};

struct Text {
    const char* name;
    QColor color;
};

// Surfaces that carry text in the side panels, bars, fields and cards.
std::vector<Surface> interfaceSurfaces(const ThemeTokens& t) {
    return {{"chrome", t.chrome}, {"bar", t.bar},       {"panel", t.panel},
            {"field", t.field},   {"field2", t.field2}, {"raise", t.raise}};
}

// Text that also appears over the 3D view and the print bed (labels, hints) uses only the
// three stronger text colours; fg-4 is for panels (see the closing note of AP-0.8).
std::vector<Surface> viewSurfaces(const ThemeTokens& t) {
    return {{"view", t.view}, {"view2", t.view2}, {"bed", t.bed}};
}

void checkAccentTokens(const ThemeTokens& t, const QString& what) {
    INFO(what.toStdString());
    CHECK(contrastRatio(t.accentFg, t.accent) >= 4.5);
    CHECK(contrastRatio(t.accentFg, t.accentHover) >= 4.5);
    const QColor soft = t.accentSoft;
    for (const QColor& surface : {t.panel, t.field, t.chrome, t.view, soft}) {
        CHECK(contrastRatio(t.accentText, surface) >= 4.5);
    }
}

} // namespace

TEST_CASE("all text tokens reach 4.5:1 on the interface surfaces", "[theme][contrast]") {
    for (const bool dark : {true, false}) {
        const ThemeTokens t = makeTokens(dark);
        const std::vector<Text> texts{{"fg", t.fg},
                                      {"fg2", t.fg2},
                                      {"fg3", t.fg3},
                                      {"fg4", t.fg4},
                                      {"accentText", t.accentText},
                                      {"amber", t.amber},
                                      {"ok", t.ok},
                                      {"bad", t.bad}};
        for (const Surface& surface : interfaceSurfaces(t)) {
            for (const Text& text : texts) {
                INFO((dark ? "dark " : "light ") << text.name << " on " << surface.name);
                CHECK(contrastRatio(text.color, surface.color) >= 4.5);
            }
        }
    }
}

TEST_CASE("text over the 3D view uses fg, fg2, fg3", "[theme][contrast]") {
    for (const bool dark : {true, false}) {
        const ThemeTokens t = makeTokens(dark);
        for (const Surface& surface : viewSurfaces(t)) {
            for (const Text& text : {Text{"fg", t.fg}, Text{"fg2", t.fg2}, Text{"fg3", t.fg3}}) {
                INFO((dark ? "dark " : "light ") << text.name << " on " << surface.name);
                CHECK(contrastRatio(text.color, surface.color) >= 4.5);
            }
        }
    }
}

TEST_CASE("state colours stay readable on their soft backgrounds", "[theme][contrast]") {
    for (const bool dark : {true, false}) {
        const ThemeTokens t = makeTokens(dark);
        INFO((dark ? "dark" : "light"));
        CHECK(contrastRatio(t.amber, composite(t.amberSoft, t.panel)) >= 4.5);
        CHECK(contrastRatio(t.ok, composite(t.okSoft, t.panel)) >= 4.5);
        CHECK(contrastRatio(t.bad, composite(t.badSoft, t.panel)) >= 4.5);
        CHECK(contrastRatio(t.amberFg, t.amber) >= 4.5);
        CHECK(contrastRatio(t.fg, t.accentSoft) >= 4.5);
        CHECK(contrastRatio(t.fg, t.field2) >= 4.5);
    }
}

TEST_CASE("the default accent keeps the values of the design table", "[theme]") {
    const ThemeTokens dark = makeTokens(true);
    CHECK(dark.accent == QColor("#2563D9"));
    CHECK(dark.accentText == QColor("#7FB0FF"));
    CHECK(dark.accentLine == QColor("#5B97FF"));
    CHECK(dark.accentSoft == QColor("#22314F"));
    const ThemeTokens light = makeTokens(false);
    CHECK(light.accentText == QColor("#1F57C2"));
    CHECK(light.accentSoft == QColor("#DCE8FF"));
    checkAccentTokens(dark, "default dark");
    checkAccentTokens(light, "default light");
}

TEST_CASE("derived accent colours keep AA, also at the extremes", "[theme][contrast]") {
    std::vector<QColor> colours;
    for (const auto& [name, colour] : accentSuggestions()) {
        colours.push_back(colour);
    }
    for (const char* extreme : {"#FFFFFF", "#000000", "#FFE600", "#B6FF6A", "#FF0000", "#00FF00"}) {
        colours.emplace_back(extreme);
    }
    for (const QColor& colour : colours) {
        for (const bool dark : {true, false}) {
            checkAccentTokens(makeTokens(dark, colour),
                              colour.name() + (dark ? " dark" : " light"));
        }
    }
}

TEST_CASE("colours that already carry a meaning get a hint", "[theme]") {
    CHECK(accentClash(QColor("#E53935")) == QStringLiteral("fails"));
    CHECK(accentClash(QColor("#F0C020")) == QStringLiteral("needs help"));
    CHECK(accentClash(QColor("#2EA043")) == QStringLiteral("fits"));
    CHECK(accentClash(QColor("#2563D9")).isEmpty());
    CHECK(accentClash(QColor("#808080")).isEmpty());
}

TEST_CASE("every filament colour is told apart from both 3D views", "[theme][contrast]") {
    const auto palette = filamentPalette();
    CHECK(palette.size() == 10);
    for (const bool dark : {true, false}) {
        const ThemeTokens tokens = makeTokens(dark);
        for (const auto& [name, colour] : palette) {
            INFO(name.toStdString() << (dark ? " dark" : " light"));
            // Black filament on the dark view and white on the light one are as dark or as bright
            // as the view itself, as in real life: shading and the edge lines show the shape.
            const bool blends = (dark && name == QLatin1String("black")) ||
                                (!dark && name == QLatin1String("white"));
            const double least = blends ? 1.0 : 1.1;
            CHECK(contrastRatio(colour, tokens.view) >= least);
            CHECK(contrastRatio(colour, tokens.view2) >= least);
        }
    }
}

TEST_CASE("the filament colours differ from each other", "[theme]") {
    const auto palette = filamentPalette();
    for (int a = 0; a < palette.size(); ++a) {
        for (int b = a + 1; b < palette.size(); ++b) {
            INFO(palette[a].first.toStdString() << " / " << palette[b].first.toStdString());
            CHECK(palette[a].second != palette[b].second);
        }
    }
}
