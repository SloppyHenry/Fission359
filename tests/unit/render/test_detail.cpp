// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fission/render/detail.h"

using Catch::Approx;
using namespace fission::render;

TEST_CASE("zooming in asks for finer meshes, zooming out for the basic one", "[detail]") {
    CHECK(detailLevelFor(1.0F) == 0);
    CHECK(detailLevelFor(10.0F) == 0);
    // A hole of 3 mm filling a screen of 1000 px is about 330 px per mm.
    CHECK(detailLevelFor(330.0F) == kMaxDetailLevel);
    // each level takes twice the scale
    CHECK(detailLevelFor(60.0F) < detailLevelFor(120.0F));
    CHECK(detailLevelFor(120.0F) < detailLevelFor(250.0F));
    CHECK(detailLevelFor(100000.0F) == kMaxDetailLevel);
    CHECK(detailLevelFor(0.0F) == 0);
}

TEST_CASE("the level does not flip back and forth at a threshold", "[detail]") {
    // Find a scale where the level goes from 1 to 2 when zooming in.
    float scale = 20.0F;
    while (detailLevelFor(scale, 1) < 2) {
        scale *= 1.01F;
    }
    // Just below that scale the finer level is kept ...
    CHECK(detailLevelFor(scale * 0.95F, 2) == 2);
    // ... and only clearly below it the view goes back.
    CHECK(detailLevelFor(scale * 0.5F, 2) < 2);
}

TEST_CASE("every level halves the chord error and the error stays under a quarter pixel",
          "[detail]") {
    const auto basic = tessellationForDetail(0);
    CHECK(basic.chordTolerance == Approx(0.01));
    CHECK(basic.angleTolerance == Approx(0.0872664626));
    for (int level = 1; level <= kMaxDetailLevel; ++level) {
        const auto finer = tessellationForDetail(level);
        CHECK(finer.chordTolerance ==
              Approx(0.5 * tessellationForDetail(level - 1).chordTolerance));
        CHECK(finer.angleTolerance < tessellationForDetail(level - 1).angleTolerance);
    }
    // At the scale a level is chosen for, its chord error is at most a quarter of a pixel.
    for (const float scale : {30.0F, 60.0F, 120.0F, 240.0F, 400.0F}) {
        const int level = detailLevelFor(scale);
        CHECK(tessellationForDetail(level).chordTolerance * scale <= 0.25 + 1e-9);
    }
    // Levels outside the range are held to it.
    CHECK(tessellationForDetail(99).chordTolerance ==
          Approx(tessellationForDetail(kMaxDetailLevel).chordTolerance));
}

// ---- acceptance: a hole of 3 mm filling the screen shows no facets --------------------------

#include <cmath>

#include "fission/geo/occt/kernel.h"

namespace {

// How far the polygon of the circle edges stays inside the true circle: the largest distance of
// a segment's midpoint from the circle (the corners are on it), in millimetres. The wall of the
// hole is made of strips between these segments, so its silhouette is off by the same amount.
bool onCircle(const fission::geo::Point3f& p, double radius) {
    return std::abs(std::hypot(p.x, p.y) - radius) < 1e-4;
}

double largestSagitta(const fission::geo::TriangleMesh& mesh, double radius) {
    double worst = 0.0;
    for (const auto& edge : mesh.edges) {
        for (std::size_t i = 0; i + 1 < edge.points.size(); ++i) {
            const auto& a = edge.points[i];
            const auto& b = edge.points[i + 1];
            if (std::abs(a.z - b.z) > 1e-6F || !onCircle(a, radius) || !onCircle(b, radius)) {
                continue; // not a piece of the circle
            }
            const double mx = 0.5 * (a.x + b.x);
            const double my = 0.5 * (a.y + b.y);
            worst = std::max(worst, radius - std::hypot(mx, my));
        }
    }
    return worst;
}

} // namespace

TEST_CASE("acceptance: a hole of 3 mm at full-screen zoom has no visible facets",
          "[detail][acceptance]") {
    const auto kernel = fission::geo::occt::makeKernel();
    // The wall of a hole is the wall of a cylinder; only the side faces point the other way.
    const auto hole = kernel->makeCylinder(1.5, 5.0, {0.0, 0.0, 0.0});

    // The hole is as tall as the screen: 1080 px for 3 mm.
    const float pixelsPerMillimetre = 1080.0F / 3.0F;
    const int level = detailLevelFor(pixelsPerMillimetre);
    const auto fine = kernel->tessellate(hole.solid, tessellationForDetail(level));
    const double fineError = largestSagitta(fine, 1.5) * pixelsPerMillimetre;
    CHECK(fineError <= 0.25);

    // Without the fine level (the basic mesh the program starts with) the facets would be
    // about a pixel: this is what the adaptive level is for.
    const auto basic = kernel->tessellate(hole.solid, tessellationForDetail(0));
    const double basicError = largestSagitta(basic, 1.5) * pixelsPerMillimetre;
    CHECK(basicError > fineError);
    INFO("basic " << basicError << " px, fine " << fineError << " px at level " << level);
    CHECK(fine.triangleCount() > basic.triangleCount());
}

TEST_CASE("detail: a big body is drawn with a coarser mesh, a small one as before",
          "[render][detail]") {
    using fission::render::tessellationForDetail;
    // Up to 100 mm the basic chord error stays 0.01 mm.
    CHECK(tessellationForDetail(0, 0.0).chordTolerance == Approx(0.01));
    CHECK(tessellationForDetail(0, 80.0).chordTolerance == Approx(0.01));
    // A sphere of 800 mm radius (box diagonal about 2770 mm): about 0.28 mm, still finer than a
    // quarter of a pixel when the whole sphere fits on the screen.
    CHECK(tessellationForDetail(0, 2771.0).chordTolerance == Approx(0.2771));
    // Zooming in still makes it finer.
    CHECK(tessellationForDetail(2, 2771.0).chordTolerance == Approx(0.2771 / 4.0));
}
