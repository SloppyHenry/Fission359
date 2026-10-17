// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The pictures on the tiles of the shape gallery (AP-1.5c).

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QImage>
#include <QPainter>

#include <cstdlib>
#include <memory>
#include <set>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/shape_previews.h"

using fission::ui::ShapePreviews;

namespace {

const char* const kTypes[] = {"box",  "cylinder", "sphere",  "hemisphere", "cone", "torus",
                              "tube", "prism",    "pyramid", "wedge",      "star", "text"};

std::shared_ptr<ShapePreviews> previews() {
    fission::geo::occt::KernelOptions options;
    options.fontFiles = {std::string(FISSION_APP_FONTS_DIR) + "/IBMPlexSans-Regular.ttf"};
    std::shared_ptr<const fission::geo::GeometryKernel> kernel =
        fission::geo::occt::makeKernel(options);
    return std::make_shared<ShapePreviews>(kernel);
}

int visiblePixels(const QImage& image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            count += qAlpha(image.pixel(x, y)) > 40 ? 1 : 0;
        }
    }
    return count;
}

} // namespace

TEST_CASE("tile pictures: every shape has a picture of its own", "[ui][previews]") {
    const auto provider = previews();
    const QSize size(186, 156);
    std::set<QByteArray> seen;
    for (const char* type : kTypes) {
        INFO(type);
        const QImage image = provider->render(QString::fromLatin1(type), false, size);
        CHECK(image.size() == size);
        // The shape fills a good part of the tile, and not all of it.
        const int visible = visiblePixels(image);
        CHECK(visible > size.width() * size.height() / 20);
        CHECK(visible < size.width() * size.height() * 3 / 4);
        // No two shapes look alike.
        seen.insert(QByteArray(reinterpret_cast<const char*>(image.constBits()),
                               static_cast<qsizetype>(image.sizeInBytes())));
        // The real shape is behind it.
        CHECK_FALSE(provider->meshOf(QString::fromLatin1(type)).empty());
    }
    CHECK(seen.size() == std::size(kTypes));
}

TEST_CASE("tile pictures: a hole is a see-through ghost", "[ui][previews]") {
    const auto provider = previews();
    const QSize size(93, 78);
    const QImage solid = provider->render(QStringLiteral("cylinder"), false, size);
    const QImage ghost = provider->render(QStringLiteral("cylinder"), true, size);
    auto strongest = [](const QImage& image) {
        int most = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                most = std::max(most, qAlpha(image.pixel(x, y)));
            }
        }
        return most;
    };
    CHECK(strongest(solid) == 255);
    CHECK(strongest(ghost) < 200);
    CHECK(visiblePixels(ghost) > 0);
}

TEST_CASE("tile pictures: an unknown shape gets a placeholder, not a crash", "[ui][previews]") {
    const auto provider = previews();
    const QImage image = provider->render(QStringLiteral("teapot"), false, QSize(62, 52));
    CHECK(image.size() == QSize(62, 52));
    CHECK(visiblePixels(image) > 0);
}

TEST_CASE("tile pictures: the picture request of the interface works", "[ui][previews]") {
    const auto provider = previews();
    QSize size;
    const QImage image =
        provider->requestImage(QStringLiteral("sphere/hole"), &size, QSize(62, 52));
    CHECK(size == QSize(62, 52));
    CHECK(image.size() == QSize(62, 52));
}

// With FISSION_PREVIEW_SHEET=<file.png> the pictures are also written as one sheet, to look at.
TEST_CASE("tile pictures: contact sheet", "[ui][previews][.sheet]") {
    const char* path = std::getenv("FISSION_PREVIEW_SHEET");
    if (path == nullptr) {
        SUCCEED("FISSION_PREVIEW_SHEET not set");
        return;
    }
    const auto provider = previews();
    const QSize tile(186, 156);
    QImage sheet(tile.width() * 6, tile.height() * 4, QImage::Format_ARGB32_Premultiplied);
    sheet.fill(QColor(0x2B, 0x2D, 0x31));
    QPainter painter(&sheet);
    int n = 0;
    for (const bool hole : {false, true}) {
        for (const char* type : kTypes) {
            painter.drawImage((n % 6) * tile.width(), (n / 6) * tile.height(),
                              provider->render(QString::fromLatin1(type), hole, tile));
            ++n;
        }
    }
    painter.end();
    CHECK(sheet.save(QString::fromUtf8(path)));
}
