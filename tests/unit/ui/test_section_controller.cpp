// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The section view (AP-2.11c): a view setting only, never part of the document.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>

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
