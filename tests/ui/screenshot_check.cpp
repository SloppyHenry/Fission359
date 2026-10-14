// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Checks a screenshot of the main window (written by `fission3d --smoke-test`):
//   screenshot_check <file.png> <dark|light> [--expect-3d]
// The surfaces of the window must have the colours of the design tokens (docs/08-gestaltung.md
// §1 and §2), and with --expect-3d the cube must be there: bright pixels on the print bed in
// the dark design, and the dark edge lines in both.

#include <QColor>
#include <QGuiApplication>
#include <QImage>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "fission/ui/theme_tokens.h"

namespace {

int failures = 0;

double distance(const QColor& a, const QColor& b) {
    return std::sqrt(std::pow(a.red() - b.red(), 2) + std::pow(a.green() - b.green(), 2) +
                     std::pow(a.blue() - b.blue(), 2));
}

// The image may be taken at 2x pixel density: positions are given in logical pixels.
struct Shot {
    QImage image;
    double scale = 1.0;
    QColor at(double x, double y) const {
        return image.pixelColor(static_cast<int>(std::lround(x * scale)),
                                static_cast<int>(std::lround(y * scale)));
    }
    int width() const { return static_cast<int>(image.width() / scale); }
    int height() const { return static_cast<int>(image.height() / scale); }
};

void expectColour(const Shot& shot, const char* what, double x, double y, const QColor& expected,
                  double tolerance = 6.0) {
    const QColor actual = shot.at(x, y);
    const double d = distance(actual, expected);
    std::printf("%-34s at (%4.0f,%4.0f): %s expected %s  (difference %.1f)\n", what, x, y,
                qPrintable(actual.name()), qPrintable(expected.name()), d);
    if (d > tolerance) {
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: screenshot_check file.png dark|light [--expect-3d]\n");
        return 2;
    }
    const bool dark = std::strcmp(argv[2], "dark") == 0;
    const bool expect3d = argc > 3 && std::strcmp(argv[3], "--expect-3d") == 0;

    Shot shot;
    shot.image = QImage(QString::fromUtf8(argv[1])).convertToFormat(QImage::Format_RGB32);
    if (shot.image.isNull()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    // The window is 1280 logical pixels wide in the tests; the picture may be bigger (Retina).
    shot.scale = shot.image.width() / 1280.0;
    std::printf("picture %d x %d, scale %.2f\n", shot.image.width(), shot.image.height(),
                shot.scale);
    if (shot.width() < 1200 || shot.height() < 700) {
        std::fprintf(stderr, "window too small: %d x %d\n", shot.width(), shot.height());
        return 1;
    }

    const fission::ui::ThemeTokens t = fission::ui::makeTokens(dark);
    const int h = shot.height();
    const int w = shot.width();
    expectColour(shot, "menu bar", 6, 6, t.chrome);
    expectColour(shot, "document bar", 6, 46, t.bar);
    expectColour(shot, "left side bar", 60, h / 2 - 40, t.panel);
    expectColour(shot, "right side bar", w - 60, h / 2 + 100, t.panel);
    expectColour(shot, "status line", w / 2 + 300, h - 8, t.chrome);

    if (expect3d) {
        // Scan the middle of the 3D view: the cube has bright faces (dark design) and always
        // dark edge lines; the bed around it is a flat dark or light grey.
        int bright = 0;
        int darkEdge = 0;
        const int x0 = 380;
        const int x1 = w - 400;
        const int y0 = 200;
        const int y1 = h - 200;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const QColor c = shot.at(x, y);
                if (dark && c.lightness() > 170) {
                    ++bright;
                }
                if (!dark && c.lightness() < 135 && c.saturation() < 40) {
                    ++darkEdge;
                }
            }
        }
        std::printf("bright pixels in the view: %d, dark edge pixels: %d\n", bright, darkEdge);
        if (dark && bright < 400) {
            std::fprintf(stderr, "the cube is not visible (too few bright pixels)\n");
            ++failures;
        }
        if (!dark && darkEdge < 40) {
            std::fprintf(stderr, "the cube is not visible (too few edge pixels)\n");
            ++failures;
        }
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("OK");
    return 0;
}
