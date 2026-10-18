// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The gizmo of the tool "Move" (AP-1.6a): arrows, squares and the middle dot move the chosen
// body; so does dragging the body itself. Every move changes the position values of the step
// that made the body (ADR-0010), is one step in the history and can be typed instead.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QPointF>
#include <QPointingDevice>
#include <QVariantMap>

#include <cmath>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/gizmo_controller.h"
#include "fission/ui/handle_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::GizmoController;
using fission::ui::HandleController;
using fission::ui::Viewport3D;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// Makes the protected input handlers reachable.
class TestViewport : public Viewport3D {
public:
    using Viewport3D::mouseMoveEvent;
    using Viewport3D::mousePressEvent;
    using Viewport3D::mouseReleaseEvent;

    void press(QPointF at, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        const auto* device = QPointingDevice::primaryPointingDevice();
        QMouseEvent event(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton,
                          modifiers, device);
        mousePressEvent(&event);
    }
    void move(QPointF at) {
        const auto* device = QPointingDevice::primaryPointingDevice();
        QMouseEvent event(QEvent::MouseMove, at, at, Qt::NoButton, Qt::LeftButton, Qt::NoModifier,
                          device);
        mouseMoveEvent(&event);
    }
    void release(QPointF at) {
        const auto* device = QPointingDevice::primaryPointingDevice();
        QMouseEvent event(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton,
                          Qt::NoModifier, device);
        mouseReleaseEvent(&event);
    }
};

struct Fixture {
    explicit Fixture(double yaw = 35.0, double pitch = 30.0, double distance = 160.0) {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 × 20 × 20 mm at (0, 0, 0)
        model->waitForCalculation();
        viewport.setProperty("animated", false);
        viewport.setWidth(1000.0);
        viewport.setHeight(600.0);
        viewport.setCamera(yaw, pitch, distance);
        viewport.setScene(model->scene());
        model->selectStep(0);
        gizmo = std::make_unique<GizmoController>(model.get());
        gizmo->attach(&viewport);
        gizmo->setEnabled(true);
    }

    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
        gizmo->refresh();
    }

    QVariantMap part(const QString& name) const {
        for (const QVariant& entry : gizmo->parts()) {
            if (entry.toMap()["part"].toString() == name) {
                return entry.toMap();
            }
        }
        return {};
    }

    QVariantMap valueEntry(const QString& name) const {
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap()["name"].toString() == name) {
                return entry.toMap();
            }
        }
        return {};
    }
    double value(const QString& name) const { return valueEntry(name)["value"].toDouble(); }

    int stepsDone() const { return model->historyPosition(); }

    // Pulls a part of the gizmo by view pixels, through the controller (as the window does).
    void pull(const QString& name, QPointF by, bool release = true) {
        const QVariantMap start = part(name);
        REQUIRE_FALSE(start.isEmpty());
        const QPointF from(start["x"].toDouble(), start["y"].toDouble());
        REQUIRE(gizmo->beginDrag(name, from.x(), from.y()));
        for (int i = 1; i <= 4; ++i) {
            gizmo->dragTo(from.x() + by.x() * i / 4, from.y() + by.y() * i / 4);
        }
        if (release) {
            REQUIRE(gizmo->endDrag());
        }
        refresh();
    }

    // Where the X arrow goes on screen (a unit vector), from the middle dot to its head.
    QPointF screenWay(const QString& name) const {
        const QVariantMap head = part(name);
        const QVariantMap middle = part(QStringLiteral("free"));
        QPointF way(head["x"].toDouble() - middle["x"].toDouble(),
                    head["y"].toDouble() - middle["y"].toDouble());
        return way / std::hypot(way.x(), way.y());
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    TestViewport viewport;
    std::unique_ptr<GizmoController> gizmo;
};

} // namespace

TEST_CASE("gizmo: the tool Move shows arrows, squares and the dot above the body", "[ui][gizmo]") {
    Fixture f;
    for (const char* name : {"x", "y", "z", "free"}) {
        CHECK_FALSE(f.part(QLatin1String(name)).isEmpty());
    }
    CHECK_FALSE(f.part(QStringLiteral("xy")).isEmpty()); // seen from above at a slant
    // Drawn in the scene: shafts, heads, squares and the dot.
    const auto& drawn = f.viewport.handleGraphics(Viewport3D::HandleSource::gizmo);
    CHECK(drawn.heads.size() == 3);
    CHECK(drawn.dots.size() == 1);
    CHECK_FALSE(drawn.quads.empty());
    // It sits above the middle of the top: the dot is above the middle of the box on screen.
    const QPointF top = f.viewport.project(0.0, 0.0, 20.0);
    const QVariantMap dot = f.part(QStringLiteral("free"));
    CHECK(dot["x"].toDouble() == Approx(top.x()).margin(2.0));
    CHECK(dot["y"].toDouble() <= top.y() + 0.5);

    f.gizmo->setEnabled(false); // another tool
    CHECK(f.gizmo->parts().isEmpty());
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::gizmo).empty());
}

TEST_CASE("gizmo: an arrow pointing at the viewer is left out", "[ui][gizmo]") {
    Fixture f(0.0, 90.0, 300.0); // straight down: Z points at the eye, XZ and YZ are edge on
    CHECK(f.part(QStringLiteral("z")).isEmpty());
    CHECK(f.part(QStringLiteral("xz")).isEmpty());
    CHECK(f.part(QStringLiteral("yz")).isEmpty());
    CHECK_FALSE(f.part(QStringLiteral("x")).isEmpty());
    CHECK_FALSE(f.part(QStringLiteral("xy")).isEmpty());
}

TEST_CASE("gizmo: the X arrow moves along X only, in whole millimetres, as one step",
          "[ui][gizmo]") {
    Fixture f;
    // A formula in the position stays where the body does not move.
    REQUIRE(f.model->setStepValue(0, QStringLiteral("y"), QStringLiteral("2 + 3")));
    f.refresh();
    const int before = f.stepsDone();

    f.pull(QStringLiteral("x"), f.screenWay(QStringLiteral("x")) * 60.0);

    const double x = f.value(QStringLiteral("x"));
    CHECK(x > 5.0);
    CHECK(x == Approx(std::round(x))); // whole millimetres
    CHECK(f.valueEntry(QStringLiteral("y"))["expression"].toString() == QStringLiteral("2 + 3"));
    CHECK(f.value(QStringLiteral("z")) == Approx(0.0));
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("moveBody"));
    CHECK(f.model->recipe().size() == 1); // no new step in the recipe (ADR-0010)

    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.value(QStringLiteral("x")) == Approx(0.0));
}

TEST_CASE("gizmo: Esc during a drag puts the body back and leaves no step", "[ui][gizmo]") {
    Fixture f;
    const int before = f.stepsDone();
    f.pull(QStringLiteral("x"), f.screenWay(QStringLiteral("x")) * 60.0, false);
    CHECK(f.gizmo->editing());
    CHECK(f.gizmo->offset()[0].toDouble() > 5.0);
    f.gizmo->cancelDrag();
    f.refresh();
    CHECK_FALSE(f.gizmo->editing());
    CHECK(f.value(QStringLiteral("x")) == Approx(0.0));
    CHECK(f.stepsDone() == before);
}

TEST_CASE("gizmo: the flat square and the dot move across the bed, never up", "[ui][gizmo]") {
    for (const char* name : {"xy", "free"}) {
        Fixture f;
        f.pull(QLatin1String(name), QPointF(50.0, 30.0));
        CHECK(std::abs(f.value(QStringLiteral("x"))) + std::abs(f.value(QStringLiteral("y"))) >
              5.0);
        CHECK(f.value(QStringLiteral("z")) == Approx(0.0));
    }
    Fixture f;
    f.pull(QStringLiteral("z"), QPointF(0.0, -40.0)); // up on the screen
    CHECK(f.value(QStringLiteral("z")) > 5.0);
    CHECK(f.value(QStringLiteral("x")) == Approx(0.0));
}

TEST_CASE("gizmo: typing replaces the drag, Tab goes from X to Y, one step for it all",
          "[ui][gizmo]") {
    Fixture f;
    const int before = f.stepsDone();
    f.pull(QStringLiteral("x"), f.screenWay(QStringLiteral("x")) * 30.0, false);
    REQUIRE(f.gizmo->typeInsteadOfDrag());
    CHECK(f.gizmo->setOffset(0, 12.0));
    CHECK(f.gizmo->setOffset(1, -3.5)); // Tab: Y joins in
    CHECK_FALSE(f.gizmo->endDrag());    // the pointer has let go already
    REQUIRE(f.gizmo->commitEdit());
    f.refresh();
    CHECK(f.value(QStringLiteral("x")) == Approx(12.0));
    CHECK(f.value(QStringLiteral("y")) == Approx(-3.5));
    CHECK(f.valueEntry(QStringLiteral("y"))["expression"].toString().isEmpty()); // a plain number
    CHECK(f.stepsDone() == before + 1);
}

TEST_CASE("gizmo: a body pressed and dragged in the view moves across the bed", "[ui][gizmo]") {
    Fixture f;
    f.viewport.setBodiesMovable(true);
    // The window glue: a drag that starts on a body moves it.
    QObject::connect(&f.viewport, &Viewport3D::bodyDragStarted, f.gizmo.get(),
                     [&f](const QString&, double x, double y) { f.gizmo->beginBodyDrag(x, y); });
    QObject::connect(&f.viewport, &Viewport3D::bodyDragMoved, f.gizmo.get(),
                     [&f](double x, double y) { f.gizmo->dragTo(x, y); });
    QObject::connect(&f.viewport, &Viewport3D::bodyDragFinished, f.gizmo.get(),
                     [&f] { f.gizmo->endDrag(); });
    bool rubberBand = false;
    QObject::connect(&f.viewport, &Viewport3D::rubberBandChanged, f.gizmo.get(),
                     [&rubberBand](const QRectF& area) { rubberBand |= !area.isEmpty(); });

    const QPointF onBody = f.viewport.project(0.0, -10.0, 10.0); // the middle of the front face
    const QPointF to = onBody + QPointF(80.0, 20.0);
    f.viewport.press(onBody);
    for (int i = 1; i <= 4; ++i) {
        f.viewport.move(onBody + (to - onBody) * i / 4.0);
    }
    f.viewport.release(to);
    f.refresh();
    CHECK_FALSE(rubberBand);
    CHECK_FALSE(f.gizmo->editing());
    CHECK(std::abs(f.value(QStringLiteral("x"))) + std::abs(f.value(QStringLiteral("y"))) > 5.0);
    CHECK(f.value(QStringLiteral("z")) == Approx(0.0));
    // The point that was grabbed stays (nearly) under the pointer.
    const QPointF grabbedNow =
        f.viewport.project(f.value(QStringLiteral("x")), f.value(QStringLiteral("y")) - 10.0, 10.0);
    CHECK(std::hypot(grabbedNow.x() - to.x(), grabbedNow.y() - to.y()) < 8.0);
}

TEST_CASE("gizmo: in the tool Select a drag on a body draws a rectangle", "[ui][gizmo]") {
    Fixture f;
    f.viewport.setBodiesMovable(false);
    bool started = false;
    bool rubberBand = false;
    QObject::connect(&f.viewport, &Viewport3D::bodyDragStarted, f.gizmo.get(),
                     [&started] { started = true; });
    QObject::connect(&f.viewport, &Viewport3D::rubberBandChanged, f.gizmo.get(),
                     [&rubberBand](const QRectF& area) { rubberBand |= !area.isEmpty(); });
    const QPointF onBody = f.viewport.project(0.0, -10.0, 10.0);
    f.viewport.press(onBody);
    f.viewport.move(onBody + QPointF(60.0, 30.0));
    f.viewport.release(onBody + QPointF(60.0, 30.0));
    CHECK_FALSE(started);
    CHECK(rubberBand);
}

TEST_CASE("gizmo: arrow keys move by 1 mm, with Shift by 10, along the bed axis on screen",
          "[ui][gizmo]") {
    Fixture f(0.0, 90.0, 300.0); // straight down: right is +X, up on the screen is +Y
    const int before = f.stepsDone();
    REQUIRE(f.gizmo->nudge(1, 0, 0, false));
    f.refresh();
    CHECK(f.value(QStringLiteral("x")) == Approx(1.0));
    REQUIRE(f.gizmo->nudge(0, 1, 0, true));
    f.refresh();
    CHECK(f.value(QStringLiteral("y")) == Approx(10.0));
    REQUIRE(f.gizmo->nudge(0, 0, 1, false));
    f.refresh();
    CHECK(f.value(QStringLiteral("z")) == Approx(1.0));
    CHECK(f.stepsDone() == before + 3); // one step each
}

TEST_CASE("handles: with the tool Move only the bands are left, with Select nothing",
          "[ui][handles][gizmo]") {
    Fixture f;
    HandleController handles(f.model.get());
    handles.attach(&f.viewport);
    handles.refresh();
    REQUIRE_FALSE(handles.handles().isEmpty());
    CHECK_FALSE(f.viewport.handleGraphics().dots.empty());

    handles.setMode(QStringLiteral("labels"));
    CHECK_FALSE(handles.handles().isEmpty()); // the labels can still be clicked to type
    CHECK(f.viewport.handleGraphics().dots.empty());
    CHECK_FALSE(f.viewport.handleGraphics().lines.empty()); // the bands
    const QVariantMap first = handles.handles().front().toMap();
    CHECK_FALSE(handles.beginDrag(0, first["x"].toDouble(), first["y"].toDouble()));

    handles.setMode(QStringLiteral("off"));
    CHECK(handles.handles().isEmpty());
    CHECK(f.viewport.handleGraphics().empty());
}
