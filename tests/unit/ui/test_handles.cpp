// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The size handles (AP-1.5d): drag and typing change values, never scale. They are placed like
// the measures of a drawing: dot on the face, band and label beside the body. Ablauf D, step 1
// (docs/02-ux-konzept.md §15): a box with rounded corners is pulled wider and its corners are
// still quarter circles of the same radius.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QVariantMap>
#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/handle_controller.h"
#include "fission/ui/placement_controller.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
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
        viewport.setCamera(0.0, 90.0, 300.0); // straight down: width and depth are on the screen
        handles = std::make_unique<HandleController>(model.get());
        handles->attach(&viewport);
    }

    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
        handles->refresh();
    }

    // The handle of a value, as the controller shows it on screen.
    QVariantMap handle(const QString& parameter) const {
        for (const QVariant& entry : handles->handles()) {
            if (entry.toMap()["parameter"].toString() == parameter) {
                return entry.toMap();
            }
        }
        FAIL("no handle for " << parameter.toStdString());
        return {};
    }

    double value(const QString& name) const {
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap()["name"].toString() == name) {
                return entry.toMap()["value"].toDouble();
            }
        }
        return -1.0;
    }

    QString recipeState() const { return model->recipe().at(0).toMap()["state"].toString(); }

    int stepsDone() const {
        int done = 0;
        for (const QVariant& step : model->history()) {
            done += step.toMap()["done"].toBool() ? 1 : 0;
        }
        return done;
    }

    // Moves the pointer by `millimetres` along the handle's own direction (a point of the world
    // under it is meant: it is a ray that is laid on the axis), through the real controller.
    void pull(const QString& parameter, double millimetres, bool release = true) {
        const QVariantMap start = handle(parameter);
        const QPointF from(start["x"].toDouble(), start["y"].toDouble());
        REQUIRE(handles->beginDrag(start["index"].toInt(), from.x(), from.y()));
        const double pixels = millimetres * start["pixelsPerMillimetre"].toDouble();
        handles->dragTo(from.x() + start["dirX"].toDouble() * pixels,
                        from.y() + start["dirY"].toDouble() * pixels);
        if (release) {
            REQUIRE(handles->endDrag());
        }
        refresh();
    }

    // Distance of every vertex of a named face from a vertical axis; all must equal `radius`.
    void checkRoundCorner(const std::string& role, double axisX, double axisY, double radius) {
        const auto result = model->result();
        REQUIRE(result != nullptr);
        const auto& body = result->bodies.begin()->second;
        fission::geo::TessellationParams fine;
        fine.chordTolerance = 0.001;
        const fission::geo::TriangleMesh mesh = kernel->tessellate(body.solid, fine);
        std::uint32_t face = 0;
        for (const auto& named : body.faces) {
            if (named.name.ends_with("/" + role)) {
                face = named.faceId;
            }
        }
        REQUIRE(face != 0);
        int checked = 0;
        for (std::size_t t = 0; t < mesh.faceIds.size(); ++t) {
            if (mesh.faceIds[t] != face) {
                continue;
            }
            for (int c = 0; c < 3; ++c) {
                const auto& p = mesh.positions[mesh.indices[3 * t + static_cast<std::size_t>(c)]];
                CHECK(std::hypot(p.x - axisX, p.y - axisY) == Approx(radius).margin(1e-4));
                ++checked;
            }
        }
        CHECK(checked > 6);
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    Viewport3D viewport;
    std::unique_ptr<HandleController> handles;
};

// A box 40 x 40 x 10 with 5 mm corners, chosen, with its handles on screen.
void roundedPlate(Fixture& f) {
    f.model->setStepValue(0, "width", "40");
    f.model->setStepValue(0, "depth", "40");
    f.model->setStepValue(0, "height", "10");
    f.model->setStepValue(0, "cornerRadius", "5");
    f.model->selectStep(0);
    f.refresh();
}

} // namespace

TEST_CASE("handles: a box shows one for each size and one for its corners", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    REQUIRE(f.handles->handles().size() == 4);
    CHECK(f.handle("width")["value"].toDouble() == Approx(40.0));
    CHECK(f.handle("cornerRadius")["value"].toDouble() == Approx(5.0));
    // On screen, inside the view.
    for (const QVariant& entry : f.handles->handles()) {
        CHECK(entry.toMap()["x"].toDouble() > 0.0);
        CHECK(entry.toMap()["x"].toDouble() < 1000.0);
    }
}

TEST_CASE("acceptance (Ablauf D, step 1): the corners stay true when the box is pulled wider",
          "[ui][handles][acceptance]") {
    Fixture f;
    roundedPlate(f);
    f.checkRoundCorner("cornerFrontLeft", -20.0 + 5.0, -20.0 + 5.0, 5.0);

    const int before = f.stepsDone();
    f.pull("width", 20.0); // the face goes 20 mm to the side: the box grows to both sides, 40 -> 80
    CHECK(f.value("width") == Approx(80.0));
    CHECK(f.value("cornerRadius") == Approx(5.0)); // the radius is its own value
    CHECK(f.stepsDone() == before + 1);            // the whole drag is one step
    CHECK(f.model->nextUndo()["key"].toString() == "setValue");
    CHECK(f.model->nextUndo()["field"].toString() == "width");
    CHECK(f.model->recipe().at(0).toMap()["state"].toString() == "ok");

    // Measure the corners of the new body: still quarter circles of 5 mm, not ellipses.
    f.checkRoundCorner("cornerFrontLeft", -40.0 + 5.0, -20.0 + 5.0, 5.0);
    f.checkRoundCorner("cornerFrontRight", 40.0 - 5.0, -20.0 + 5.0, 5.0);
    f.checkRoundCorner("cornerBackLeft", -40.0 + 5.0, 20.0 - 5.0, 5.0);
    f.checkRoundCorner("cornerBackRight", 40.0 - 5.0, 20.0 - 5.0, 5.0);
    const auto report = f.kernel->measure(f.model->result()->bodies.begin()->second.solid);
    CHECK(report.valid);
    CHECK(report.volume ==
          Approx((80.0 * 40.0 - (4.0 - std::numbers::pi) * 25.0) * 10.0).epsilon(1e-6));
}

TEST_CASE("handles: Esc during a drag takes everything back", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    const int before = f.stepsDone();
    f.pull("width", 25.0, /*release=*/false);
    CHECK(f.handles->editing());
    f.handles->cancelDrag();
    f.refresh();
    CHECK_FALSE(f.handles->editing());
    CHECK(f.value("width") == Approx(40.0));
    CHECK(f.stepsDone() == before);
}

TEST_CASE("handles: a drag stops where the shape stops being possible", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    // Pulled far below twice the corner radius: the width must stay possible, and the step valid.
    f.pull("width", -100.0);
    CHECK(f.value("width") >= 10.0 - 1e-9); // 2 x 5 mm
    CHECK(f.recipeState() == "ok");
}

TEST_CASE("handles: looking along a handle, it is dragged up and down the screen",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    // From straight above, the height handle points at the eye: its whole length is a point.
    // It used to be locked; now the pointer going up the screen makes the box taller, at the rate
    // of a millimetre across the screen (steady, never racing away).
    const QVariantMap height = f.handle("height");
    CHECK(height["pixelsPerMillimetre"].toDouble() < 0.5);
    const QPointF dot(height["x"].toDouble(), height["y"].toDouble());
    const QPointF across = f.viewport.project(1.0, 0.0, 10.0) - f.viewport.project(0.0, 0.0, 10.0);
    const double perMillimetre = 0.75 * std::hypot(across.x(), across.y());
    REQUIRE(f.handles->beginDrag(height["index"].toInt(), dot.x(), dot.y()));
    f.handles->dragTo(dot.x(), dot.y() - 15.0 * perMillimetre);
    CHECK(f.value("height") == Approx(10.0)); // not calculated yet: the drag is still on
    f.handles->dragTo(dot.x(), dot.y() - 15.0 * perMillimetre);
    REQUIRE(f.handles->endDrag());
    f.refresh();
    CHECK(f.value("height") == Approx(25.0).margin(1.0));
}

TEST_CASE("handles: typing replaces dragging, Tab goes on, one step for it all", "[ui][handles]") {
    Fixture f;
    f.model->selectStep(0);
    f.refresh();
    REQUIRE(f.handles->beginEdit(0));
    CHECK(f.handles->editing());
    CHECK(f.handles->setValue(0, "50"));       // width
    CHECK(f.handles->setValue(1, "2 cm + 5")); // depth, as a formula
    CHECK(f.handles->setValue(2, "4,5"));      // height, with a comma
    CHECK(f.handles->commitEdit());
    f.refresh();

    CHECK(f.value("width") == Approx(50.0));
    CHECK(f.value("depth") == Approx(25.0));
    CHECK(f.value("height") == Approx(4.5));
    CHECK(f.stepsDone() == 1);

    // Esc during typing leaves things as they were.
    REQUIRE(f.handles->beginEdit(0));
    f.handles->setValue(0, "99");
    f.handles->cancelEdit();
    f.refresh();
    CHECK(f.value("width") == Approx(50.0));
    CHECK(f.stepsDone() == 1);
}

TEST_CASE("handles: a shape just put down waits for typing, until something else is chosen",
          "[ui][handles]") {
    Fixture f;
    CHECK_FALSE(f.handles->armed());
    const int step = f.model->addShape("box", false);
    f.refresh();
    CHECK(f.handles->armed());
    CHECK(f.model->currentStep() == step);

    f.model->selectStep(0); // chose another step: digits are for the views again
    CHECK_FALSE(f.handles->armed());
}

TEST_CASE("handles: the snap follows the zoom", "[ui][handles]") {
    CHECK(HandleController::snapStep(100.0) == Approx(0.1));
    CHECK(HandleController::snapStep(10.0) == Approx(0.5));
    CHECK(HandleController::snapStep(5.0) == Approx(1.0));
    CHECK(HandleController::snapStep(1.5) == Approx(1.0)); // a foreshortened edge: still whole mm
    CHECK(HandleController::snapStep(0.5) == Approx(5.0));
    CHECK(HandleController::snapStep(0.2) == Approx(10.0));
}

TEST_CASE("handles: typing a number while dragging takes over from the pointer", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    const int before = f.stepsDone();
    f.pull("width", 10.0, /*release=*/false); // a drag is going on ...
    REQUIRE(f.handles->typeInsteadOfDrag());  // ... a digit is typed: the pointer is let go
    CHECK(f.handles->editing());
    CHECK(f.handles->setValue(0, "60"));
    CHECK(f.handles->commitEdit());
    f.refresh();

    CHECK(f.value("width") == Approx(60.0));
    CHECK(f.stepsDone() == before + 1);          // the drag and the typing are one step
    CHECK_FALSE(f.handles->typeInsteadOfDrag()); // nothing is dragged any more
}

TEST_CASE("handles: the dot stays under the pointer while the shape follows", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    const QVariantMap start = f.handle("width");
    const double perMillimetre = start["pixelsPerMillimetre"].toDouble();
    REQUIRE(perMillimetre > 1.0);
    const QPointF from(start["x"].toDouble(), start["y"].toDouble());
    REQUIRE(f.handles->beginDrag(start["index"].toInt(), from.x(), from.y()));

    // The face moves 15 mm to the right: the dot is where the pointer is, at once (before the
    // shape is calculated again) and at the end (when it is).
    const QPointF pointer(from.x() + 15.0 * perMillimetre, from.y());
    f.handles->dragTo(pointer.x(), pointer.y());
    const double snapPixels = HandleController::snapStep(perMillimetre) * perMillimetre;
    CHECK(std::abs(f.handle("width")["x"].toDouble() - pointer.x()) <= snapPixels);
    f.handles->endDrag();
    f.refresh();
    CHECK(std::abs(f.handle("width")["x"].toDouble() - pointer.x()) <= snapPixels);
    // And the box grew to both sides: 15 mm on the face, 30 mm in the width.
    CHECK(f.value("width") == Approx(70.0).margin(HandleController::snapStep(perMillimetre)));
}

TEST_CASE("handles: a handle that points almost at the viewer moves steadily, sizes stay sane",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    // Looking steeply from above, the height handle points almost at the eye: 60 pixels of
    // movement once made the box 70 mm taller (then it was locked). Now 60 pixels are what they
    // are across the screen.
    f.viewport.setCamera(0.0, 70.0, 300.0);
    f.refresh();
    const double before = f.value("height");
    const QPointF across = f.viewport.project(1.0, 0.0, 10.0) - f.viewport.project(0.0, 0.0, 10.0);
    const double perMillimetre = 0.75 * std::hypot(across.x(), across.y());
    const QVariantMap height = f.handle("height");
    const QPointF dot(height["x"].toDouble(), height["y"].toDouble());
    REQUIRE(f.handles->beginDrag(height["index"].toInt(), dot.x(), dot.y()));
    f.handles->dragTo(dot.x() + 60.0 * height["dirX"].toDouble(),
                      dot.y() + 60.0 * height["dirY"].toDouble());
    REQUIRE(f.handles->endDrag());
    f.refresh();
    CHECK(f.value("height") > before);
    CHECK(f.value("height") <= before + 60.0 / perMillimetre + 1.0);

    // From the side the same handle is dragged normally, and never beyond a sane size.
    f.viewport.setCamera(0.0, 10.0, 300.0);
    f.refresh();
    f.pull("height", 5000.0);
    CHECK(f.value("height") <= HandleController::kLargestMillimetres);
    CHECK(f.recipeState() == "ok");
}

TEST_CASE("handles: pulled smaller than possible, a size stops at the smallest it can be",
          "[ui][handles]") {
    // Before, the size stayed where it was: a box pulled far smaller could not be made smaller at
    // all, and after a few rounds of bigger and smaller it filled the view.
    Fixture f;
    f.model->selectStep(0);
    f.viewport.setCamera(0.0, 90.0, 300.0);
    f.refresh();
    f.pull("width", 30.0, false); // 20 -> 80 (a box grows to both sides)
    CHECK(f.handles->editing());
    const QVariantMap width = f.handle("width");
    const QPointF dot(width["x"].toDouble(), width["y"].toDouble());
    const double perMillimetre = width["pixelsPerMillimetre"].toDouble();
    // Far back across the middle of the box: the width would be below zero.
    f.handles->dragTo(dot.x() - 200.0 * perMillimetre * width["dirX"].toDouble(),
                      dot.y() - 200.0 * perMillimetre * width["dirY"].toDouble());
    REQUIRE(f.handles->endDrag());
    f.refresh();
    CHECK(f.value("width") > 0.0);
    CHECK(f.value("width") <= 1.0);
    CHECK(f.recipeState() == "ok");
}

TEST_CASE("handles: rounding a corner goes in whole millimetres, also in a slanted view",
          "[ui][handles]") {
    Fixture f;
    f.model->setStepValue(0, "width", "40");
    f.model->setStepValue(0, "depth", "40");
    f.model->setStepValue(0, "cornerRadius", "0");
    f.model->selectStep(0);
    // The edge the handle runs along is foreshortened here; the steps were 5 mm once.
    f.viewport.setCamera(-30.0, 36.0, 260.0);
    f.refresh();

    double last = 0.0;
    int changes = 0;
    const QVariantMap start = f.handle("cornerRadius");
    const QPointF from(start["x"].toDouble(), start["y"].toDouble());
    REQUIRE(f.handles->beginDrag(start["index"].toInt(), from.x(), from.y()));
    for (int step = 1; step <= 40; ++step) {
        // Along the handle's way on screen: its own direction, a little more each time.
        const double pixels = step * 0.25 * start["pixelsPerMillimetre"].toDouble();
        const double value = f.handles->dragTo(from.x() + start["dirX"].toDouble() * pixels,
                                               from.y() + start["dirY"].toDouble() * pixels);
        CHECK(value - last <= 1.0 + 1e-9); // never a jump of more than a millimetre
        changes += value != last ? 1 : 0;
        last = value;
    }
    f.handles->endDrag();
    f.refresh();
    CHECK(changes >= 8);         // 10 mm in the steps of one
    CHECK(last == Approx(10.0)); // the pointer went 10 mm along the edge: the radius followed
}

TEST_CASE("handles: each value is taken on the side that faces the viewer", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    const auto placeOf = [&](const QString& parameter) {
        const int candidate = f.handle(parameter)["candidate"].toInt();
        return f.model->stepHandles()[candidate].toMap();
    };

    f.viewport.setCamera(0.0, 36.0, 300.0); // from the front (-Y)
    f.refresh();
    CHECK(placeOf("depth")["y"].toDouble() < 0.0);
    f.viewport.setCamera(180.0, 36.0, 300.0); // from the back
    f.refresh();
    CHECK(placeOf("depth")["y"].toDouble() > 0.0);
    f.viewport.setCamera(90.0, 36.0, 300.0); // from the side: the width is taken on that side
    f.refresh();
    const double sideA = placeOf("width")["x"].toDouble();
    f.viewport.setCamera(-90.0, 36.0, 300.0);
    f.refresh();
    CHECK(placeOf("width")["x"].toDouble() == Approx(-sideA));

    // The value of the handle does not depend on the side: the same width either way.
    CHECK(f.handle("width")["value"].toDouble() == Approx(40.0));

    // One handle for each value, whatever the number of places.
    CHECK(f.handles->handles().size() == 4);
}

TEST_CASE("handles: pulling the handle on the far side changes the value the same way",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    f.viewport.setCamera(180.0, 90.0, 300.0); // straight down, turned round: +x points left
    f.refresh();
    f.pull("width", 10.0); // the face goes 10 mm outwards on whichever side the handle is
    CHECK(f.value("width") == Approx(60.0));
}

namespace {

// Whether a screen point is inside the outline of the box round the chosen box (its eight
// corners as seen on screen).
bool onTheBody(Fixture& f, const QPointF& point) {
    const double w = f.value("width");
    const double d = f.value("depth");
    const double h = f.value("height");
    const double x = f.value("x");
    const double y = f.value("y");
    const double z = f.value("z");
    std::vector<QPointF> corners;
    for (int corner = 0; corner < 8; ++corner) {
        corners.push_back(f.viewport.project(x + ((corner & 1) != 0 ? w : -w) / 2.0,
                                             y + ((corner & 2) != 0 ? d : -d) / 2.0,
                                             z + ((corner & 4) != 0 ? h : 0.0)));
    }
    // The outline is the convex hull of the corners (monotone chain).
    std::sort(corners.begin(), corners.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });
    const auto turn = [](const QPointF& o, const QPointF& a, const QPointF& b) {
        return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
    };
    std::vector<QPointF> hull(2 * corners.size());
    std::size_t k = 0;
    for (const QPointF& p : corners) {
        while (k >= 2 && turn(hull[k - 2], hull[k - 1], p) <= 0.0) {
            --k;
        }
        hull[k++] = p;
    }
    for (std::size_t i = corners.size() - 1, low = k + 1; i-- > 0;) {
        while (k >= low && turn(hull[k - 2], hull[k - 1], corners[i]) <= 0.0) {
            --k;
        }
        hull[k++] = corners[i];
    }
    hull.resize(k - 1);
    for (std::size_t i = 0; i < hull.size(); ++i) {
        if (turn(hull[i], hull[(i + 1) % hull.size()], point) < 0.0) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("handles: the dot sits on the face it moves, the band and its label beside the body",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    for (const double yaw : {-35.0, 30.0, 150.0, -120.0}) {
        f.viewport.setCamera(yaw, 32.0, 260.0);
        f.refresh();
        for (const QString& parameter :
             {QStringLiteral("width"), QStringLiteral("depth"), QStringLiteral("height")}) {
            CAPTURE(yaw, parameter.toStdString());
            const QVariantMap shown = f.handle(parameter);
            const QVariantMap place = f.model->stepHandles()[shown["candidate"].toInt()].toMap();
            const QPointF onFace = f.viewport.project(place["x"].toDouble(), place["y"].toDouble(),
                                                      place["z"].toDouble());
            CHECK(shown["x"].toDouble() == Approx(onFace.x()).margin(0.5));
            CHECK(shown["y"].toDouble() == Approx(onFace.y()).margin(0.5));
            REQUIRE(shown["band"].toBool());
            const QPointF bandStart(shown["bandStartX"].toDouble(), shown["bandStartY"].toDouble());
            const QPointF bandEnd(shown["bandEndX"].toDouble(), shown["bandEndY"].toDouble());
            CHECK_FALSE(onTheBody(f, (bandStart + bandEnd) / 2.0));
            CHECK_FALSE(onTheBody(f, {shown["chipX"].toDouble(), shown["chipY"].toDouble()}));
            // As long as what it measures.
            const QPointF edgeStart(shown["edgeStartX"].toDouble(), shown["edgeStartY"].toDouble());
            const QPointF edgeEnd(shown["edgeEndX"].toDouble(), shown["edgeEndY"].toDouble());
            const double band =
                std::hypot(bandEnd.x() - bandStart.x(), bandEnd.y() - bandStart.y());
            const double edge =
                std::hypot(edgeEnd.x() - edgeStart.x(), edgeEnd.y() - edgeStart.y());
            CHECK(band == Approx(edge).epsilon(0.25));
        }
    }
}

TEST_CASE("handles: sizes have axis colours, a rounding is a quiet detail", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    f.viewport.setCamera(-35.0, 32.0, 260.0);
    f.refresh();
    CHECK(f.handle("width")["axis"].toString() == "x");
    CHECK(f.handle("depth")["axis"].toString() == "y");
    CHECK(f.handle("height")["axis"].toString() == "z");
    CHECK(f.handle("width")["role"].toString() == "size");
    CHECK(f.handle("cornerRadius")["role"].toString() == "rounding");
    CHECK(f.handle("cornerRadius")["axis"].toString() == "none"); // not blue like the height
    CHECK_FALSE(f.handle("cornerRadius")["band"].toBool());
    CHECK_FALSE(onTheBody(f, {f.handle("cornerRadius")["chipX"].toDouble(),
                              f.handle("cornerRadius")["chipY"].toDouble()}));
}

TEST_CASE("handles: looked at end on, a size has no band and its label is beside the body",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f); // the fixture looks straight down: the height points at the eye
    CHECK_FALSE(f.handle("height")["band"].toBool());
    CHECK(f.handle("width")["band"].toBool());
    CHECK_FALSE(onTheBody(
        f, {f.handle("height")["chipX"].toDouble(), f.handle("height")["chipY"].toDouble()}));
}

TEST_CASE("handles: a body small on screen shows its dots only", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    f.viewport.setCamera(-35.0, 32.0, 260.0);
    f.refresh();
    CHECK_FALSE(f.handles->compact());
    f.viewport.setCamera(-35.0, 32.0, 3000.0);
    f.refresh();
    CHECK(f.handles->compact());
}

TEST_CASE("handles: the classic style shows white squares and the bands only on hover",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    f.viewport.setCamera(-35.0, 32.0, 260.0);
    f.refresh();
    REQUIRE(f.handles->style() == QStringLiteral("axes"));
    CHECK_FALSE(f.handles->compact());
    CHECK_FALSE(f.viewport.handleGraphics().dots[0].square);
    const std::size_t axesLines = f.viewport.handleGraphics().lines.size();
    REQUIRE(axesLines > 0);

    f.handles->setStyle(QStringLiteral("nonsense")); // refused
    CHECK(f.handles->style() == QStringLiteral("axes"));
    f.handles->setStyle(QStringLiteral("classic"));
    f.refresh();
    const auto& classic = f.viewport.handleGraphics();
    REQUIRE(classic.dots.size() == 4);
    CHECK(f.handles->compact()); // squares only, the bands wait for the pointer
    CHECK(classic.lines.empty());
    for (const auto& dot : classic.dots) {
        if (!dot.rounding) {
            CHECK(dot.square);
            CHECK(dot.fill == QColor(Qt::white));
        }
    }
    // The same handles, the same values: only the look differs.
    CHECK(f.handle("width")["value"].toDouble() > 0.0);
    f.handles->setPointed(f.handle("width")["index"].toInt(), true);
    CHECK(f.viewport.handleGraphics().lines.size() == 5); // its band and strokes appear
    f.handles->setStyle(QStringLiteral("axes"));
    f.refresh();
    CHECK(f.viewport.handleGraphics().lines.size() == axesLines);
}

TEST_CASE("handles: a radius is taken at the outline, where it is seen from the side",
          "[ui][handles]") {
    Fixture f;
    f.model->addShape("cylinder", false);
    f.refresh();
    for (const double yaw : {-35.0, 0.0, 70.0, 200.0}) {
        f.viewport.setCamera(yaw, 30.0, 260.0);
        f.refresh();
        CAPTURE(yaw);
        const QVariantMap shown = f.handle("radius");
        const QVariantMap place = f.model->stepHandles()[shown["candidate"].toInt()].toMap();
        const QVector3D at(static_cast<float>(place["x"].toDouble()),
                           static_cast<float>(place["y"].toDouble()),
                           static_cast<float>(place["z"].toDouble()));
        const QVector3D way(static_cast<float>(place["dx"].toDouble()),
                            static_cast<float>(place["dy"].toDouble()), 0.0F);
        const QVector3D toEye = f.viewport.eyePosition() - at;
        const QVector3D flat = QVector3D(toEye.x(), toEye.y(), 0.0F).normalized();
        // Facing the viewer or at the outline, never behind; and well round to the side (the
        // places are 45 degrees apart).
        CHECK(QVector3D::dotProduct(way, toEye) >= 0.0F);
        CHECK(std::abs(QVector3D::dotProduct(way, flat)) < 0.75F);
    }
}

TEST_CASE("handles: dots, bands and arrows are drawn in the scene, the arrow on hover",
          "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    f.viewport.setCamera(-35.0, 32.0, 260.0);
    f.refresh();
    const auto& graphics = f.viewport.handleGraphics();
    CHECK(graphics.dots.size() == 4);   // width, depth, height, corner radius
    CHECK(graphics.lines.size() == 15); // three bands of five strokes each
    CHECK(graphics.arrows.empty());     // nothing pointed at yet
    // Every dot sits where its handle is in the scene.
    const QVariantMap width = f.handle("width");
    const QVariantMap place = f.model->stepHandles()[width["candidate"].toInt()].toMap();
    CHECK(graphics.dots[0].centre.x() == Approx(place["x"].toDouble()));

    f.handles->setPointed(width["index"].toInt(), true);
    REQUIRE(f.viewport.handleGraphics().arrows.size() == 1);
    CHECK(f.viewport.handleGraphics().dots[0].radius > graphics.dots[1].radius);
    f.handles->setPointed(width["index"].toInt(), false);
    CHECK(f.viewport.handleGraphics().arrows.empty());

    // The corner radius shows what it measures while the pointer is on it.
    f.handles->setPointed(f.handle("cornerRadius")["index"].toInt(), true);
    CHECK(f.viewport.handleGraphics().lines.size() == 15 + 2);
    CHECK(f.viewport.handleGraphics().dots[3].rounding);
}

TEST_CASE("handles: a label steps back when a body is in front of its band", "[ui][handles]") {
    Fixture f;
    roundedPlate(f);
    f.viewport.setCamera(-35.0, 32.0, 260.0);
    f.refresh();
    CHECK_FALSE(f.handle("width")["hidden"].toBool());
    // A tall wall right in front of the box, between it and the eye.
    const int wall = f.model->addShape("box", false);
    f.model->setStepValue(wall, "width", "200");
    f.model->setStepValue(wall, "depth", "4");
    f.model->setStepValue(wall, "height", "80");
    f.model->setStepValue(wall, "y", "-60");
    f.model->selectStep(0);
    f.refresh();
    CHECK(f.handle("width")["hidden"].toBool());
}

TEST_CASE("handles: the radius dot of a sphere stays under the pointer as the sphere grows",
          "[ui][handles]") {
    // Feedback 2026-10-18: the sphere stands on the bed, so its equator rises as it grows. The
    // dot was pulled only sideways and slid away from the pointer.
    Fixture f;
    f.model->addShape(QStringLiteral("sphere"), false); // radius 10, on a free place of the bed
    f.model->selectStep(1);
    f.viewport.setCamera(-30.0, 30.0, 250.0);
    f.refresh();
    const QVariantMap start = f.handle("radius");
    const QPointF from(start["x"].toDouble(), start["y"].toDouble());
    REQUIRE(f.handles->beginDrag(start["index"].toInt(), from.x(), from.y()));
    // Where the dot will be at radius 20: 10 mm out and 10 mm up from where it was.
    const QVector3D origin(
        static_cast<float>(
            f.model->stepHandles()[start["candidate"].toInt()].toMap()["x"].toDouble()),
        static_cast<float>(
            f.model->stepHandles()[start["candidate"].toInt()].toMap()["y"].toDouble()),
        static_cast<float>(
            f.model->stepHandles()[start["candidate"].toInt()].toMap()["z"].toDouble()));
    const QVariantMap world = f.model->stepHandles()[start["candidate"].toInt()].toMap();
    const QVector3D out(static_cast<float>(world["dx"].toDouble()),
                        static_cast<float>(world["dy"].toDouble()), 0.0F);
    const QVector3D later = origin + out * 10.0F + QVector3D(0.0F, 0.0F, 10.0F);
    const QPointF pointer = f.viewport.project(later.x(), later.y(), later.z());
    f.handles->dragTo(pointer.x(), pointer.y());
    // At once, and once the sphere is calculated again (still holding it): under the pointer.
    for (int pass = 0; pass < 2; ++pass) {
        const QVariantMap now = f.handle("radius");
        CHECK(std::hypot(now["x"].toDouble() - pointer.x(), now["y"].toDouble() - pointer.y()) <
              6.0);
        f.refresh();
    }
    REQUIRE(f.handles->endDrag());
    f.refresh();
    CHECK(f.value("radius") == Approx(20.0).margin(1.0));
    // Let go, it stays where it is (it once jumped to another place on the ring).
    const QVariantMap after = f.handle("radius");
    CHECK(std::hypot(after["x"].toDouble() - pointer.x(), after["y"].toDouble() - pointer.y()) <
          6.0);
}

// --- catching (AP-1.6c2) ---

namespace {

// Snapping is off as if Shift were held, until the end of the scope.
struct SnapOff {
    SnapOff() { fission::ui::setSnappingSuppressedForTests(true); }
    ~SnapOff() { fission::ui::setSnappingSuppressedForTests(false); }
    SnapOff(const SnapOff&) = delete;
    SnapOff& operator=(const SnapOff&) = delete;
};

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

// The demo box in the middle, a box on each side of it at a place that is not a whole millimetre.
struct Neighbours : Fixture {
    Neighbours() {
        for (const char* x : {"75.3", "-75.3"}) {
            const int step = model->addShape(QStringLiteral("box"), false);
            REQUIRE(step >= 0);
            REQUIRE(model->setStepValue(step, QStringLiteral("x"), QLatin1String(x)));
            REQUIRE(model->setStepValue(step, QStringLiteral("y"), QStringLiteral("0")));
            REQUIRE(model->setStepValue(step, QStringLiteral("z"), QStringLiteral("0")));
        }
        model->selectStep(0);
        refresh();
    }

    // Takes the handle of `parameter` and goes on with the pointer, snapping off, until the value
    // is `target` (to a thousandth). Stays holding the handle; returns where the pointer is. The
    // caller lets snapping have its say by switching it on and calling `dragTo` again.
    QPointF steer(const QString& parameter, double target) {
        const QVariantMap start = handle(parameter);
        const QPointF from(start["x"].toDouble(), start["y"].toDouble());
        const QPointF way(start["dirX"].toDouble(), start["dirY"].toDouble());
        const double perMillimetre = start["pixelsPerMillimetre"].toDouble();
        REQUIRE(handles->beginDrag(start["index"].toInt(), from.x(), from.y()));
        double pixels = 0.0;
        double value = handles->dragTo(from.x(), from.y());
        double slope = 1.0 / perMillimetre; // value per pixel; better after the first move
        for (int i = 0; i < 12 && std::abs(target - value) > 0.001; ++i) {
            const double next = pixels + (target - value) / slope;
            const double nextValue =
                handles->dragTo(from.x() + way.x() * next, from.y() + way.y() * next);
            if (std::abs(next - pixels) > 1e-9 && std::abs(nextValue - value) > 1e-9) {
                slope = (nextValue - value) / (next - pixels);
            }
            pixels = next;
            value = nextValue;
        }
        REQUIRE(value == Approx(target).margin(0.002));
        return {from.x() + way.x() * pixels, from.y() + way.y() * pixels};
    }

    // The face the handle of `width` moves: its side (+1 right, -1 left) and where it stands.
    double faceOfWidth() const {
        const QVariantMap world =
            model->stepHandles().at(handle("width")["candidate"].toInt()).toMap();
        return world["x"].toDouble();
    }
    // The side of the neighbour across from that face.
    double neighbourSide() const {
        return faceOfWidth() > 0.0 ? extentOf(*model, 1).low[0] : extentOf(*model, 2).high[0];
    }
};

} // namespace

TEST_CASE("snap: a size handle catches the face on the side of another body",
          "[ui][handles][snap]") {
    Neighbours f;
    const double side = f.neighbourSide();
    const double face = f.faceOfWidth();
    // The face 0.4 mm short of the other body: the width this is (it grows to both sides).
    const double short_ = (std::abs(side) - 0.4);
    const QPointF pointer = [&] {
        SnapOff off;
        return f.steer(QStringLiteral("width"), 2.0 * short_);
    }();
    const std::size_t dotsBefore =
        f.viewport.handleGraphics(Viewport3D::HandleSource::size).dots.size();
    f.handles->dragTo(pointer.x(), pointer.y()); // snapping is on again

    // While held: a mark on the point caught, the grid on the bed.
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::size).dots.size() > dotsBefore);
    CHECK(f.viewport.snapGridStep() > 0.0);
    REQUIRE(f.handles->endDrag());
    f.refresh();

    // The sides touch, exactly: not 0.4 mm short, and not on a whole millimetre either.
    CHECK(f.value(QStringLiteral("width")) == Approx(2.0 * std::abs(side)).margin(1e-4));
    const Extent mine = extentOf(*f.model, 0);
    CHECK((face > 0.0 ? mine.high[0] : mine.low[0]) == Approx(side).margin(1e-4));
    CHECK(f.viewport.snapGridStep() == 0.0);
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::size).dots.size() <= dotsBefore);
}

TEST_CASE("snap: a size handle with Shift held is free", "[ui][handles][snap]") {
    Neighbours f;
    const double short_ = std::abs(f.neighbourSide()) - 0.4;
    SnapOff off;
    const QPointF pointer = f.steer(QStringLiteral("width"), 2.0 * short_);
    f.handles->dragTo(pointer.x(), pointer.y());
    CHECK(f.viewport.snapGridStep() == 0.0); // no grid either
    REQUIRE(f.handles->endDrag());
    f.refresh();
    // Where the pointer put it, 0.4 mm short; neither flush nor on a whole millimetre.
    CHECK(f.value(QStringLiteral("width")) == Approx(2.0 * short_).margin(0.002));
}

TEST_CASE("snap: out of reach a size handle goes to the grid", "[ui][handles][snap]") {
    Neighbours f;
    const double short_ = std::abs(f.neighbourSide()) - 13.4;
    const QPointF pointer = [&] {
        SnapOff off;
        return f.steer(QStringLiteral("width"), 2.0 * short_);
    }();
    f.handles->dragTo(pointer.x(), pointer.y());
    REQUIRE(f.handles->endDrag());
    f.refresh();
    const double width = f.value(QStringLiteral("width"));
    CHECK(width == Approx(std::round(width)));
    CHECK(width < 2.0 * std::abs(f.neighbourSide()) - 20.0);
}

TEST_CASE("snap: typing a size is not caught on anything", "[ui][handles][snap]") {
    Neighbours f;
    const double short_ = std::abs(f.neighbourSide()) - 0.4;
    const QPointF pointer = [&] {
        SnapOff off;
        return f.steer(QStringLiteral("width"), 2.0 * short_);
    }();
    f.handles->dragTo(pointer.x(), pointer.y());
    REQUIRE(f.handles->typeInsteadOfDrag());
    CHECK(f.viewport.snapGridStep() == 0.0);
    const int index = f.handle("width")["index"].toInt();
    CHECK(f.handles->setValue(index, QStringLiteral("37.3")));
    REQUIRE(f.handles->commitEdit());
    f.refresh();
    CHECK(f.value(QStringLiteral("width")) == Approx(37.3));
}

TEST_CASE("snap: one catching drag is one step in the history and Esc takes it back",
          "[ui][handles][snap]") {
    Neighbours f;
    const int before = f.stepsDone();
    const double short_ = std::abs(f.neighbourSide()) - 0.4;
    const QPointF pointer = [&] {
        SnapOff off;
        return f.steer(QStringLiteral("width"), 2.0 * short_);
    }();
    f.handles->dragTo(pointer.x(), pointer.y());
    f.handles->cancelDrag();
    f.refresh();
    CHECK(f.value(QStringLiteral("width")) == Approx(20.0));
    CHECK(f.stepsDone() == before);
    CHECK(f.viewport.snapGridStep() == 0.0);

    const QPointF again = [&] {
        SnapOff off;
        return f.steer(QStringLiteral("width"), 2.0 * short_);
    }();
    f.handles->dragTo(again.x(), again.y());
    REQUIRE(f.handles->endDrag());
    f.refresh();
    CHECK(f.stepsDone() == before + 1);
}

TEST_CASE("snap: the radius of a sphere, which rises as it grows, has only the grid",
          "[ui][handles][snap]") {
    Fixture f;
    const int sphere = f.model->addShape(QStringLiteral("sphere"), false);
    REQUIRE(sphere >= 0);
    REQUIRE(f.model->setStepValue(sphere, QStringLiteral("x"), QStringLiteral("0")));
    REQUIRE(f.model->setStepValue(sphere, QStringLiteral("y"), QStringLiteral("0")));
    REQUIRE(f.model->setStepValue(sphere, QStringLiteral("z"), QStringLiteral("0")));
    REQUIRE(f.model->setStepValue(sphere, QStringLiteral("radius"), QStringLiteral("10")));
    const int box = f.model->addShape(QStringLiteral("box"), false);
    REQUIRE(f.model->setStepValue(box, QStringLiteral("x"), QStringLiteral("22.3")));
    REQUIRE(f.model->setStepValue(box, QStringLiteral("y"), QStringLiteral("0")));
    REQUIRE(f.model->setStepValue(box, QStringLiteral("z"), QStringLiteral("0")));
    f.model->selectStep(sphere);
    f.refresh();

    const QVariantMap start = f.handle(QStringLiteral("radius"));
    const QPointF from(start["x"].toDouble(), start["y"].toDouble());
    const QPointF way(start["dirX"].toDouble(), start["dirY"].toDouble());
    REQUIRE(f.handles->beginDrag(start["index"].toInt(), from.x(), from.y()));
    double value = 10.0;
    {
        SnapOff off; // steer to a radius that is not a whole millimetre
        double pixels = 0.0;
        for (int i = 0; i < 12 && std::abs(value - 12.35) > 0.001; ++i) {
            pixels += (12.35 - value) * start["pixelsPerMillimetre"].toDouble() * 0.7;
            value = f.handles->dragTo(from.x() + way.x() * pixels, from.y() + way.y() * pixels);
        }
        REQUIRE(value == Approx(12.35).margin(0.01));
        const double steered = pixels;
        // Snapping on again: the grid, never the side of the box (12.3 would be that).
        fission::ui::setSnappingSuppressedForTests(false);
        const double caught =
            f.handles->dragTo(from.x() + way.x() * steered, from.y() + way.y() * steered);
        fission::ui::setSnappingSuppressedForTests(true); // (the guard puts it back)
        CHECK(caught == Approx(std::round(caught)));
    }
    REQUIRE(f.handles->endDrag());
}
