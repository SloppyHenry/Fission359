// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The section view (AP-2.11c): a view setting only, never part of the document.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>

#include <array>
#include <cmath>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/section_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::SectionController;
using fission::ui::Viewport3D;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct World {
    std::shared_ptr<const fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    DocumentModel model{kernel};
    SectionController section{&model, kernel};
    Viewport3D viewport;

    World() {
        application();
        model.addDemoBox(); // 20 x 20 x 20 mm
        model.waitForCalculation();
        section.attach(&viewport);
        viewport.setScene(model.scene());
    }
};

} // namespace

TEST_CASE("section: on puts the plane through the middle, perpendicular to X", "[section-ui]") {
    World w;
    CHECK_FALSE(w.section.enabled());
    CHECK_FALSE(w.viewport.clipPlane().has_value());
    w.section.setEnabled(true);
    REQUIRE(w.section.enabled());
    REQUIRE(w.viewport.clipPlane().has_value());
    const auto plane = *w.viewport.clipPlane();
    CHECK(plane.normal.x() == Approx(1.0F)); // the +X side is cut away
    CHECK(plane.point.x() == Approx(w.section.rangeMin() + 10.0));
    CHECK(w.section.rangeMax() - w.section.rangeMin() == Approx(20.0));
    w.section.setEnabled(false);
    CHECK_FALSE(w.viewport.clipPlane().has_value());
}

TEST_CASE("section: axis, position and side", "[section-ui]") {
    World w;
    w.section.setEnabled(true);
    w.section.setAxis(QStringLiteral("z"));
    CHECK(w.viewport.clipPlane()->normal.z() == Approx(1.0F));
    // The position is kept inside the bodies.
    w.section.setPosition(1000.0);
    CHECK(w.section.position() == Approx(w.section.rangeMax()));
    w.section.setPosition(-1000.0);
    CHECK(w.section.position() == Approx(w.section.rangeMin()));
    w.section.setPosition(w.section.rangeMin() + 5.0);
    CHECK(w.viewport.clipPlane()->point.z() == Approx(w.section.rangeMin() + 5.0));
    w.section.flipSide();
    CHECK(w.section.side() == -1);
    CHECK(w.viewport.clipPlane()->normal.z() == Approx(-1.0F));
    // A wrong axis name changes nothing.
    w.section.setAxis(QStringLiteral("q"));
    CHECK(w.section.axis() == QStringLiteral("z"));
}

TEST_CASE("section: a new or opened document turns it off, and it leaves no undo step",
          "[section-ui]") {
    World w;
    const int before = w.model.historyPosition();
    w.section.setEnabled(true);
    w.section.setAxis(QStringLiteral("y"));
    w.section.flipSide();
    CHECK(w.model.historyPosition() == before);
    CHECK_FALSE(w.model.canUndo());
    emit w.model.documentReplaced();
    CHECK_FALSE(w.section.enabled());
    CHECK_FALSE(w.viewport.clipPlane().has_value());
}

TEST_CASE("section: without a visible body it stays off and says so", "[section-ui]") {
    World w;
    w.model.setBodyVisible(0, false);
    int told = 0;
    QObject::connect(&w.section, &SectionController::nothingToCut, [&told] { ++told; });
    w.section.setEnabled(true);
    CHECK_FALSE(w.section.enabled());
    CHECK(told == 1);
}

namespace {

double capArea(const fission::render::CapSet& caps) {
    double area = 0.0;
    for (const fission::render::CapMesh& cap : caps) {
        for (std::size_t i = 0; i + 2 < cap.indices.size(); i += 3) {
            const auto at = [&cap](std::uint32_t index) {
                return std::array<double, 3>{cap.positions[3 * index], cap.positions[3 * index + 1],
                                             cap.positions[3 * index + 2]};
            };
            const auto a = at(cap.indices[i]);
            const auto b = at(cap.indices[i + 1]);
            const auto c = at(cap.indices[i + 2]);
            const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
            const double v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            const double x = u[1] * v[2] - u[2] * v[1];
            const double y = u[2] * v[0] - u[0] * v[2];
            const double z = u[0] * v[1] - u[1] * v[0];
            area += 0.5 * std::sqrt(x * x + y * y + z * z);
        }
    }
    return area;
}

} // namespace

TEST_CASE("section: the cut face of a cube is worked out in the background", "[section-ui]") {
    World w;
    w.section.setEnabled(true);
    // Until the result is there the viewport shows no cut face.
    CHECK(w.viewport.sectionCaps() == nullptr);
    REQUIRE(w.section.waitForCaps(10000));
    REQUIRE(w.viewport.sectionCaps() != nullptr);
    CHECK(w.viewport.sectionCaps()->size() == 1);
    CHECK(capArea(*w.viewport.sectionCaps()) == Approx(400.0).epsilon(1e-6));
    // The plane moves: the old cut face is hidden at once, the new one comes.
    w.section.setPosition(w.section.rangeMin() + 5.0);
    CHECK(w.viewport.sectionCaps() == nullptr);
    REQUIRE(w.section.waitForCaps(10000));
    CHECK(capArea(*w.viewport.sectionCaps()) == Approx(400.0).epsilon(1e-6));
    // Off: no cut face.
    w.section.setEnabled(false);
    CHECK(w.viewport.sectionCaps() == nullptr);
}

TEST_CASE("section: an old result is dropped when a newer request is there", "[section-ui]") {
    World w;
    w.section.setEnabled(true);
    w.section.setPosition(w.section.rangeMin() + 2.0);
    w.section.setPosition(w.section.rangeMin() + 8.0);
    w.section.setAxis(QStringLiteral("z"));
    REQUIRE(w.section.waitForCaps(10000));
    REQUIRE(w.viewport.sectionCaps() != nullptr);
    // Only the last plane counts: all its points lie at the middle of Z.
    for (const fission::render::CapMesh& cap : *w.viewport.sectionCaps()) {
        for (std::size_t i = 2; i < cap.positions.size(); i += 3) {
            CHECK(cap.positions[i] == Approx(w.section.position()).margin(1e-4));
        }
    }
}
