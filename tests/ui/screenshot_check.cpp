// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Checks a screenshot of the main window (written by `fission3d --smoke-test`):
//   screenshot_check <file.png> <dark|light> [--expect-3d] [--reference mockup.png]
// The surfaces of the window must have the colours of the design tokens (docs/08-gestaltung.md
// §1 and §2), and with --expect-3d the cube must be there: bright pixels on the print bed in
// the dark design, and the dark edge lines in both. With --reference the surfaces are compared
// block by block with a picture of the design mockup (docs/08-gestaltung.md §8).

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
    bool expect3d = false;
    bool nativeMenuBar = false; // macOS: the menus are in the system menu bar, not in the picture
    const char* referencePath = nullptr;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--expect-3d") == 0) {
            expect3d = true;
        } else if (std::strcmp(argv[i], "--native-menu-bar") == 0) {
            nativeMenuBar = true;
        } else if (std::strcmp(argv[i], "--reference") == 0 && i + 1 < argc) {
            referencePath = argv[++i];
        }
    }

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

    if (referencePath != nullptr) {
        Shot reference;
        reference.image =
            QImage(QString::fromUtf8(referencePath)).convertToFormat(QImage::Format_RGB32);
        if (reference.image.isNull()) {
            std::fprintf(stderr, "cannot read reference %s\n", referencePath);
            return 2;
        }
        reference.scale = reference.image.width() / 1280.0;
        // The surfaces around the 3D view: the same average colour per 8 x 8 block, except where
        // the two pictures show different texts (document name, body names): a few percent of
        // the blocks may differ.
        struct Region {
            const char* name;
            int x0, y0, x1, y1;
            bool bottomAnchored; // the picture may be lower than the reference (smaller screen)
        };
        const int viewTop = 92;
        const int statusTop = h - 28;
        // The lower end of the side bars (the recipe) sits at the bottom edge: it is left out,
        // so that a window that is a little lower than the reference can still be compared.
        const int panelBottom = statusTop - 200;
        const Region regions[] = {{"menu bar", 0, 0, w, 40, false},
                                  {"document bar", 0, 40, w, viewTop, false},
                                  {"left side bar", 0, viewTop, 248, panelBottom, false},
                                  {"right side bar", w - 300, viewTop, w, panelBottom, false},
                                  {"status line", 0, statusTop, w, h, true}};
        const int heightDifference = reference.height() - h;
        constexpr int kBlock = 8;
        for (const Region& region : regions) {
            if (nativeMenuBar && std::strcmp(region.name, "menu bar") == 0) {
                continue;
            }
            const int referenceShift = region.bottomAnchored ? heightDifference : 0;
            int blocks = 0;
            int different = 0;
            for (int y = region.y0; y + kBlock <= region.y1; y += kBlock) {
                for (int x = region.x0; x + kBlock <= region.x1; x += kBlock) {
                    double sums[2][3] = {};
                    for (int j = 0; j < kBlock; ++j) {
                        for (int i = 0; i < kBlock; ++i) {
                            const QColor a = shot.at(x + i, y + j);
                            const QColor b = reference.at(x + i, y + j + referenceShift);
                            sums[0][0] += a.red();
                            sums[0][1] += a.green();
                            sums[0][2] += a.blue();
                            sums[1][0] += b.red();
                            sums[1][1] += b.green();
                            sums[1][2] += b.blue();
                        }
                    }
                    const double n = kBlock * kBlock;
                    const QColor a(static_cast<int>(sums[0][0] / n),
                                   static_cast<int>(sums[0][1] / n),
                                   static_cast<int>(sums[0][2] / n));
                    const QColor b(static_cast<int>(sums[1][0] / n),
                                   static_cast<int>(sums[1][1] / n),
                                   static_cast<int>(sums[1][2] / n));
                    ++blocks;
                    if (distance(a, b) > 14.0) {
                        ++different;
                    }
                }
            }
            const double share = blocks > 0 ? 100.0 * different / blocks : 0.0;
            std::printf("compared with the mockup: %-15s %4d of %4d blocks differ (%.1f %%)\n",
                        region.name, different, blocks, share);
            // The thin status line is almost only text, which differs more than the surfaces.
            const double allowed = std::strcmp(region.name, "status line") == 0 ? 35.0 : 15.0;
            if (share > allowed) {
                std::fprintf(stderr, "%s differs from the mockup\n", region.name);
                ++failures;
            }
        }
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("OK");
    return 0;
}
