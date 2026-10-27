// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The distance measures of the tool "Move" (AP-1.6d): bands from the chosen body to its
// neighbours and the edge of the bed, with numbers that can be typed. Acceptance criterion 1 of
// AP-1.6: a shape stands exactly "5 mm from the edge" by a measure.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QVariantMap>

#include <cmath>
#include <limits>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/distance_controller.h"
#include "fission/ui/document_model.h"
#include "fission/ui/gizmo_controller.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DistanceController;
using fission::ui::DocumentModel;
using fission::ui::GizmoController;
using fission::ui::Viewport3D;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// Snapping is off as if Shift were held, until the end of the scope.
struct SnapOff {
    SnapOff() { fission::ui::setSnappingSuppressedForTests(true); }
    ~SnapOff() { fission::ui::setSnappingSuppressedForTests(false); }
    SnapOff(const SnapOff&) = delete;
    SnapOff& operator=(const SnapOff&) = delete;
};

// The demo box (20 mm, in the middle) chosen, with the tool "Move".
struct Fixture {
    Fixture() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox();
        model->waitForCalculation();
        viewport.setProperty("animated", false);
        viewport.setWidth(1000.0);
        viewport.setHeight(600.0);
        viewport.setCamera(35.0, 30.0, 160.0);
        viewport.setScene(model->scene());
        model->selectStep(0);
        gizmo = std::make_unique<GizmoController>(model.get());
        gizmo->attach(&viewport);
        gizmo->setEnabled(true);
        distances = std::make_unique<DistanceController>(model.get());
        distances->attach(&viewport);
        distances->attachGizmo(gizmo.get());
        distances->setEnabled(true);
    }

    // A second box at a place, 20 mm like the first.
    int addBox(const char* x, const char* y) {
        const int step = model->addShape(QStringLiteral("box"), false);
        REQUIRE(step >= 0);
        REQUIRE(model->setStepValue(step, QStringLiteral("x"), QLatin1String(x)));
        REQUIRE(model->setStepValue(step, QStringLiteral("y"), QLatin1String(y)));
        REQUIRE(model->setStepValue(step, QStringLiteral("z"), QStringLiteral("0")));
        model->selectStep(0);
        refresh();
        return step;
    }

    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
        gizmo->refresh();
        distances->refresh();
    }

    // The measure of a side as the window shows it, or an empty map.
    QVariantMap label(const char* side) const {
        for (const QVariant& entry : distances->labels()) {
            if (entry.toMap()["side"].toString() == QLatin1String(side)) {
                return entry.toMap();
            }
        }
        return {};
    }
    double gap(const char* side) const {
        const QVariantMap entry = label(side);
        return entry.contains(QStringLiteral("value")) ? entry["value"].toDouble() : -1.0;
    }

    QVariantMap valueEntry(const QString& name) const {
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap()["name"].toString() == name) {
                return entry.toMap();
            }
        }
        return {};
    }
    double value(const char* name) const {
        return valueEntry(QLatin1String(name))["value"].toDouble();
    }
    QString expression(const char* name) const {
        return valueEntry(QLatin1String(name))["expression"].toString();
    }
    int stepsDone() const { return model->historyPosition(); }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    Viewport3D viewport;
    std::unique_ptr<GizmoController> gizmo;
    std::unique_ptr<DistanceController> distances;
};

} // namespace

TEST_CASE("distances: a chosen body shows a measure in each direction", "[ui][distance]") {
    Fixture f;
    f.addBox("-27", "0"); // its right side 7 mm from the left side of the first
    REQUIRE(f.distances->labels().size() == 4);
    CHECK(f.gap("left") == Approx(7.0));
    CHECK_FALSE(f.label("left")["toBed"].toBool());
    // The others go to the edge of the bed (256 mm by default).
    CHECK(f.gap("right") == Approx(128.0 - 10.0));
    CHECK(f.label("right")["toBed"].toBool());
    CHECK(f.gap("front") == Approx(118.0));
    CHECK(f.gap("back") == Approx(118.0));
    // Where they are on the screen: inside the view.
    for (const QVariant& entry : f.distances->labels()) {
        CHECK(entry.toMap()["x"].toDouble() > 0.0);
        CHECK(entry.toMap()["x"].toDouble() < 1000.0);
    }
    // The bands are drawn in the scene: 3 strokes and 2 heads for each.
    const auto& drawn = f.viewport.handleGraphics(Viewport3D::HandleSource::distances);
    CHECK(drawn.lines.size() == 12);
    CHECK(drawn.heads.size() >= 4);
}

TEST_CASE("distances: nothing is shown with another tool or without a body that moves",
          "[ui][distance]") {
    Fixture f;
    f.distances->setEnabled(false);
    CHECK(f.distances->labels().isEmpty());
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::distances).empty());
    f.distances->setEnabled(true);
    CHECK(f.distances->labels().size() == 4);
    f.model->selectStep(-1);
    f.refresh();
    CHECK(f.distances->labels().isEmpty());
}

TEST_CASE("acceptance (AP-1.6, criterion 1): a measure sets '5 mm from the edge'",
          "[ui][distance][acceptance]") {
    Fixture f;
    const int before = f.stepsDone();
    const int left = f.distances->indexOfSide(QStringLiteral("left"));
    REQUIRE(left >= 0);
    CHECK(f.distances->setDistance(left, 5.0));
    f.refresh();
    // 20 mm wide: its left side is at -128 + 5, its middle at -113.
    CHECK(f.value("x") == Approx(-113.0));
    CHECK(f.gap("left") == Approx(5.0).margin(1e-6));
    // One move: one step in the history.
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("moveBody"));
    // And the other way round.
    const int back = f.distances->indexOfSide(QStringLiteral("back"));
    CHECK(f.distances->setDistance(back, 5.0));
    f.refresh();
    CHECK(f.value("y") == Approx(128.0 - 5.0 - 10.0));
    CHECK(f.gap("back") == Approx(5.0).margin(1e-6));
}

TEST_CASE("distances: a measure to a body is set the same way, and undo takes it back",
          "[ui][distance]") {
    Fixture f;
    f.addBox("-27", "0");
    const int before = f.stepsDone();
    CHECK(f.distances->setDistance(f.distances->indexOfSide(QStringLiteral("left")), 5.0));
    f.refresh();
    CHECK(f.gap("left") == Approx(5.0).margin(1e-6));
    CHECK(f.value("x") == Approx(-2.0)); // 2 mm nearer
    CHECK(f.stepsDone() == before + 1);
    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.value("x") == Approx(0.0));
    CHECK(f.gap("left") == Approx(7.0));
    // Also further away than before.
    CHECK(f.distances->setDistance(f.distances->indexOfSide(QStringLiteral("left")), 12.0));
    f.refresh();
    CHECK(f.gap("left") == Approx(12.0).margin(1e-6));
    CHECK(f.value("x") == Approx(5.0));
}

TEST_CASE("distances: a formula in the other direction stays when only one changes",
          "[ui][distance]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("y"), QStringLiteral("10 - 10")));
    f.refresh();
    CHECK(f.distances->setDistance(f.distances->indexOfSide(QStringLiteral("left")), 5.0));
    f.refresh();
    CHECK(f.value("x") == Approx(-113.0));
    CHECK(f.expression("y") == QStringLiteral("10 - 10"));
}

TEST_CASE("distances: a number that cannot be a distance changes nothing", "[ui][distance]") {
    Fixture f;
    f.addBox("-27", "0");
    const int before = f.stepsDone();
    const int left = f.distances->indexOfSide(QStringLiteral("left"));
    CHECK_FALSE(f.distances->setDistance(left, -3.0));
    CHECK_FALSE(f.distances->setDistance(left, std::numeric_limits<double>::quiet_NaN()));
    CHECK_FALSE(f.distances->setDistance(99, 5.0));
    CHECK_FALSE(f.distances->setDistance(-1, 5.0));
    f.refresh();
    CHECK(f.stepsDone() == before);
    CHECK(f.value("x") == Approx(0.0));
}

TEST_CASE("distances: the numbers run along while the body is moved", "[ui][distance]") {
    Fixture f;
    f.addBox("-27", "0");
    f.model->selectStep(0);
    f.refresh();
    // Take the X arrow and go on with the pointer, snapping off, until the body has moved 3 mm.
    SnapOff off;
    QVariantMap head;
    for (const QVariant& part : f.gizmo->parts()) {
        if (part.toMap()["part"].toString() == QLatin1String("x")) {
            head = part.toMap();
        }
    }
    REQUIRE_FALSE(head.isEmpty());
    QPointF pointer(head["x"].toDouble(), head["y"].toDouble());
    REQUIRE(f.gizmo->beginDrag(QStringLiteral("x"), pointer.x(), pointer.y()));
    const QPointF a = f.viewport.project(0.0, 0.0, 20.0);
    const QPointF b = f.viewport.project(1.0, 0.0, 20.0);
    const QPointF perMillimetre = b - a;
    for (int i = 0; i < 30; ++i) {
        const double error = 3.0 - f.gizmo->offset()[0].toDouble();
        if (std::abs(error) < 0.01) {
            break;
        }
        pointer += perMillimetre * error;
        f.gizmo->dragTo(pointer.x(), pointer.y());
    }
    REQUIRE(f.gizmo->offset()[0].toDouble() == Approx(3.0).margin(0.05));
    // The body is calculated a moment later; the number is already the new one.
    CHECK(f.gap("left") == Approx(7.0 + f.gizmo->offset()[0].toDouble()).margin(1e-6));
    CHECK(f.gap("right") == Approx(118.0 - f.gizmo->offset()[0].toDouble()).margin(1e-6));
    // A measure cannot be typed into while the body is being moved.
    CHECK_FALSE(f.distances->setDistance(f.distances->indexOfSide(QStringLiteral("left")), 5.0));
    f.gizmo->cancelDrag();
    f.refresh();
    CHECK(f.gap("left") == Approx(7.0));
}

TEST_CASE("distances: holes and the body itself are no neighbours", "[ui][distance]") {
    Fixture f;
    const int hole = f.model->addShape(QStringLiteral("box"), true);
    REQUIRE(hole >= 0);
    REQUIRE(f.model->setStepValue(hole, QStringLiteral("x"), QStringLiteral("-27")));
    REQUIRE(f.model->setStepValue(hole, QStringLiteral("y"), QStringLiteral("0")));
    REQUIRE(f.model->setStepValue(hole, QStringLiteral("z"), QStringLiteral("0")));
    f.model->selectStep(0);
    f.refresh();
    CHECK(f.label("left")["toBed"].toBool());
    CHECK(f.gap("left") == Approx(118.0));
}
