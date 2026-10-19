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
            if (entry.toMap()["part"].toString() == name && entry.toMap()["shown"].toBool()) {
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

TEST_CASE("gizmo: the arrow that is held stays while the body moves far, the list keeps its places",
          "[ui][gizmo]") {
    // Looking low along Y, the Y arrow is short; pulled far back, it turns ever more towards the
    // viewer. It used to be taken out of the list then, and the window let go of the drag.
    Fixture f(0.0, 25.0, 160.0);
    const QVariantMap y = f.part(QStringLiteral("y"));
    REQUIRE_FALSE(y.isEmpty());
    const QPointF from(y["x"].toDouble(), y["y"].toDouble());
    REQUIRE(f.gizmo->beginDrag(QStringLiteral("y"), from.x(), from.y()));
    const auto count = f.gizmo->parts().size();
    CHECK(count == 7);
    for (int step = 1; step <= 30; ++step) {
        f.gizmo->dragTo(from.x(), from.y() - 6.0 * step); // up the screen: away from the viewer
        CHECK(f.gizmo->parts().size() == count);
        CHECK_FALSE(f.part(QStringLiteral("y")).isEmpty());
    }
    CHECK(f.gizmo->editing());
    REQUIRE(f.gizmo->endDrag());
    f.refresh();
    CHECK(f.value(QStringLiteral("y")) > 10.0);
}

// --- snapping (AP-1.6c) ---

namespace {

// The box round a body of the scene, by its place in the list of bodies.
struct Extent {
    double low[3] = {1e30, 1e30, 1e30};
    double high[3] = {-1e30, -1e30, -1e30};
};

Extent extentOf(const DocumentModel& model, int body) {
    Extent extent;
    const std::string id = model.bodyIdAt(body).toStdString();
    for (const auto& candidate : model.scene()->bodies) {
        if (candidate.id != id) {
            continue;
        }
        for (const auto& p : candidate.mesh->positions) {
            const double v[3] = {p.x, p.y, p.z};
            for (int a = 0; a < 3; ++a) {
                extent.low[a] = std::min(extent.low[a], v[a]);
                extent.high[a] = std::max(extent.high[a], v[a]);
            }
        }
    }
    return extent;
}

// A second box beside the first, at a place that is not a whole millimetre.
struct Neighbour : Fixture {
    Neighbour() {
        const int second = model->addShape(QStringLiteral("box"), false);
        REQUIRE(second >= 0);
        REQUIRE(model->setStepValue(second, QStringLiteral("x"), QStringLiteral("75.3")));
        REQUIRE(model->setStepValue(second, QStringLiteral("y"), QStringLiteral("0")));
        REQUIRE(model->setStepValue(second, QStringLiteral("z"), QStringLiteral("0")));
        model->selectStep(0);
        refresh();
        gap = extentOf(*model, 1).low[0] - extentOf(*model, 0).high[0];
    }

    // Pulls the X arrow of the first body until it has moved `millimetres` along X (found by
    // trying: the pointer is moved on until the body is there, with snapping off), then lets
    // snapping have its say with the pointer where it is. Stays holding the arrow.
    // `shift`: snapping stays off.
    void pullX(double millimetres, bool shift = false) {
        const QVariantMap head = part(QStringLiteral("x"));
        REQUIRE_FALSE(head.isEmpty());
        QPointF pointer(head["x"].toDouble(), head["y"].toDouble());
        REQUIRE(gizmo->beginDrag(QStringLiteral("x"), pointer.x(), pointer.y()));
        gizmo->setSnapSuppressed(true);
        const Extent mine = extentOf(*model, 0);
        const double midX = (mine.low[0] + mine.high[0]) / 2.0;
        const double midY = (mine.low[1] + mine.high[1]) / 2.0;
        const QPointF a = viewport.project(midX, midY, mine.high[2]);
        const QPointF b = viewport.project(midX + 1.0, midY, mine.high[2]);
        const QPointF perMillimetre = b - a;
        for (int i = 0; i < 30; ++i) {
            const double error = millimetres - gizmo->offset()[0].toDouble();
            if (std::abs(error) < 0.01) {
                break;
            }
            pointer += perMillimetre * error;
            gizmo->dragTo(pointer.x(), pointer.y());
        }
        REQUIRE(gizmo->offset()[0].toDouble() == Approx(millimetres).margin(0.05));
        gizmo->setSnapSuppressed(shift);
        gizmo->dragTo(pointer.x(), pointer.y());
    }

    double gap = 0.0; ///< free space between the two boxes along X
};

} // namespace

TEST_CASE("snap: dragging an arrow catches the side of a body on the side of another",
          "[ui][gizmo][snap]") {
    Neighbour f;
    REQUIRE(f.gap > 10.0);
    f.pullX(f.gap - 0.8); // 0.8 mm short of touching

    // While held: a mark on the point caught, the grid on the bed.
    CHECK(f.gizmo->editing());
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::gizmo).dots.size() > 1);
    CHECK(f.viewport.snapGridStep() > 0.0);

    REQUIRE(f.gizmo->endDrag());
    f.refresh();
    // The sides touch, exactly: not 0.8 mm short, and not on a whole millimetre either.
    CHECK(extentOf(*f.model, 0).high[0] == Approx(extentOf(*f.model, 1).low[0]).margin(1e-5));
    CHECK(f.value(QStringLiteral("x")) == Approx(f.gap).margin(1e-5));
    // Gone with the drag.
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::gizmo).dots.size() == 1);
    CHECK(f.viewport.snapGridStep() == 0.0);
}

TEST_CASE("snap: out of reach the body goes to the grid", "[ui][gizmo][snap]") {
    Neighbour f;
    f.pullX(f.gap - 12.7); // far from the other body
    REQUIRE(f.gizmo->endDrag());
    f.refresh();
    const double x = f.value(QStringLiteral("x"));
    CHECK(x == Approx(std::round(x)));
    CHECK(extentOf(*f.model, 0).high[0] < extentOf(*f.model, 1).low[0] - 5.0);
}

TEST_CASE("snap: Shift switches the catching and the grid off", "[ui][gizmo][snap]") {
    Neighbour f;
    f.pullX(f.gap - 0.8, true);
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::gizmo).dots.size() == 1); // no mark
    CHECK(f.viewport.snapGridStep() == 0.0);
    REQUIRE(f.gizmo->endDrag());
    f.refresh();
    // It stops where the pointer put it: short of the other body, by about the 0.8 mm.
    const double left = extentOf(*f.model, 1).low[0] - extentOf(*f.model, 0).high[0];
    CHECK(left > 0.3);
    CHECK(left < 1.3);
}

TEST_CASE("snap: typing a number is not caught on anything", "[ui][gizmo][snap]") {
    Neighbour f;
    f.pullX(f.gap - 0.8);
    REQUIRE(f.gizmo->typeInsteadOfDrag());
    CHECK(f.viewport.snapGridStep() == 0.0);
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::gizmo).dots.size() == 1);
    CHECK(f.gizmo->setOffset(0, 12.34));
    REQUIRE(f.gizmo->commitEdit());
    f.refresh();
    CHECK(f.value(QStringLiteral("x")) == Approx(12.34));
}

TEST_CASE("snap: the grid on the bed can be switched off in the view", "[ui][gizmo][snap]") {
    Neighbour f;
    f.viewport.setSnapGridShown(false);
    f.pullX(f.gap - 0.8);
    CHECK(f.viewport.snapGridStep() > 0.0); // the step is known, the view does not show it
    CHECK_FALSE(f.viewport.snapGridShown());
    REQUIRE(f.gizmo->endDrag());
}

TEST_CASE("snap: dragging the body itself catches across the bed, the height stays",
          "[ui][gizmo][snap]") {
    Neighbour f;
    const Extent before = extentOf(*f.model, 0);
    const double midY = (before.low[1] + before.high[1]) / 2.0;
    // Grab the middle of the front face and go along X until the sides are 0.8 mm apart.
    QPointF pointer = f.viewport.project(0.0, before.low[1], 10.0);
    REQUIRE(f.gizmo->beginBodyDrag(pointer.x(), pointer.y()));
    f.gizmo->setSnapSuppressed(true);
    const QPointF a = f.viewport.project(0.0, before.low[1], 10.0);
    const QPointF b = f.viewport.project(1.0, before.low[1], 10.0);
    for (int i = 0; i < 30; ++i) {
        const double error = (f.gap - 0.8) - f.gizmo->offset()[0].toDouble();
        if (std::abs(error) < 0.01) {
            break;
        }
        pointer += (b - a) * error;
        f.gizmo->dragTo(pointer.x(), pointer.y());
    }
    f.gizmo->setSnapSuppressed(false);
    f.gizmo->dragTo(pointer.x(), pointer.y());
    REQUIRE(f.gizmo->endDrag());
    f.refresh();
    CHECK(extentOf(*f.model, 0).high[0] == Approx(extentOf(*f.model, 1).low[0]).margin(1e-5));
    CHECK(f.value(QStringLiteral("z")) == Approx(0.0));
    CHECK(midY ==
          Approx((extentOf(*f.model, 0).low[1] + extentOf(*f.model, 0).high[1]) / 2.0)
              .margin(1.5)); // across the bed it only went along X (and may have lined up in Y)
}

TEST_CASE("snap: the step of the grid can be chosen from 10 mm down to 0.01 mm",
          "[ui][gizmo][snap]") {
    // Far from the other body, so that only the grid decides.
    for (const double step : {10.0, 5.0, 0.1, 0.05, 0.01}) {
        Neighbour f;
        f.viewport.setFixedSnapStep(step);
        CHECK(f.viewport.snapStep() == Approx(step));
        f.pullX(f.gap - 23.7);
        REQUIRE(f.gizmo->endDrag());
        f.refresh();
        const double x = f.value(QStringLiteral("x"));
        const double times = x / step;
        INFO("step " << step << " x " << x);
        CHECK(std::abs(times - std::round(times)) < 1e-6);
        // The pointer wanted 23.7 mm less than the gap: the grid is as near as it can be.
        CHECK(std::abs(x - (f.gap - 23.7)) <= step / 2.0 + 0.1);
    }
}

TEST_CASE("snap: the chosen step is kept until automatic is chosen again", "[ui][gizmo][snap]") {
    Neighbour f;
    CHECK(f.viewport.fixedSnapStep() == 0.0);
    f.viewport.setFixedSnapStep(0.01);
    CHECK(f.viewport.fixedSnapStep() == Approx(0.01));
    f.viewport.setFixedSnapStep(-3.0); // not a step: automatic
    CHECK(f.viewport.fixedSnapStep() == 0.0);
    CHECK(f.viewport.snapStep() == f.viewport.snapStepFor(f.viewport.pixelsPerMillimetre()));
}
