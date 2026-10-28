// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The tool "Rotate" (AP-1.6b): three rings turn the chosen body about the axes of the bed. Every
// turn changes the angle values of the step that made the body (ADR-0010), is one step in the
// history, rests in steps of 15 degrees and can be typed instead.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/geo/rotation.h"
#include "fission/ui/document_model.h"
#include "fission/ui/rotate_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::RotateController;
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
        rings = std::make_unique<RotateController>(model.get());
        rings->attach(&viewport);
        rings->setEnabled(true);
    }

    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
        rings->refresh();
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

    QVariantMap ring(const char* name) const {
        for (const QVariant& entry : rings->rings()) {
            if (entry.toMap()["part"].toString() == QLatin1String(name)) {
                return entry.toMap();
            }
        }
        return {};
    }

    // Takes the ring where it is easiest to grab and goes `degrees` round it on the screen.
    void turn(const char* name, double degrees, bool release = true) {
        const QString part = QLatin1String(name);
        const double from = rings->grabAngle(part);
        const QPointF start = rings->ringPoint(part, from);
        REQUIRE(rings->beginDrag(part, start.x(), start.y()));
        for (int i = 1; i <= 8; ++i) {
            const QPointF at = rings->ringPoint(part, from + degrees * i / 8.0);
            rings->dragTo(at.x(), at.y());
        }
        if (release) {
            REQUIRE(rings->endDrag());
        }
        refresh();
    }

    // The box round the body as it is on screen now.
    struct Extent {
        double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30, minZ = 1e30, maxZ = -1e30;
    };
    Extent extent() const {
        Extent box;
        for (const auto& body : model->scene()->bodies) {
            for (const auto& p : body.mesh->positions) {
                box.minX = std::min<double>(box.minX, p.x);
                box.maxX = std::max<double>(box.maxX, p.x);
                box.minY = std::min<double>(box.minY, p.y);
                box.maxY = std::max<double>(box.maxY, p.y);
                box.minZ = std::min<double>(box.minZ, p.z);
                box.maxZ = std::max<double>(box.maxZ, p.z);
            }
        }
        return box;
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    Viewport3D viewport;
    std::unique_ptr<RotateController> rings;
};

} // namespace

TEST_CASE("rotate: the tool shows three rings round the middle of the body", "[ui][rotate]") {
    Fixture f;
    REQUIRE(f.rings->rings().size() == 3);
    for (const char* name : {"x", "y", "z"}) {
        CHECK(f.ring(name)["shown"].toBool());
    }
    const auto& drawn = f.viewport.handleGraphics(Viewport3D::HandleSource::rings);
    CHECK(drawn.lines.size() == 3 * RotateController::kRingSegments);
    // A ring can be taken where it is: on its line, not far from it.
    const QVariantMap z = f.ring("z");
    const QPointF onRing(z["grabX"].toDouble(), z["grabY"].toDouble());
    CHECK(f.rings->ringContains(2, onRing));
    CHECK_FALSE(f.rings->ringContains(2, onRing + QPointF(60, 60)));

    f.rings->setEnabled(false); // another tool
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::rings).empty());
    CHECK_FALSE(f.rings->ringContains(2, onRing));
}

TEST_CASE("rotate: the Z ring turns the body flat on the bed, one value, in steps of 15 degrees",
          "[ui][rotate]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotX"), QStringLiteral("5 + 5 - 10")));
    f.refresh(); // a formula that comes out as 0
    const int before = f.stepsDone();

    f.turn("z", 70.0);

    const double z = f.value("rotZ");
    CHECK(std::abs(z) > 30.0);
    CHECK(std::fmod(std::abs(z), 15.0) == Approx(0.0).margin(1e-9));
    CHECK(f.value("rotX") == Approx(0.0).margin(1e-9));
    CHECK(f.value("rotY") == Approx(0.0).margin(1e-9));
    // The formula in a value that did not change stays.
    CHECK(f.expression("rotX") == QStringLiteral("5 + 5 - 10"));
    CHECK(f.value("x") == Approx(0.0));
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("rotateBody"));
    CHECK(f.model->recipe().size() == 1); // no new step in the recipe (ADR-0010)

    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.value("rotZ") == Approx(0.0));
}

TEST_CASE("rotate: the ring turns the right way round (counter-clockwise is plus)",
          "[ui][rotate]") {
    Fixture f;
    f.turn("z", 90.0);
    CHECK(f.value("rotZ") == Approx(90.0));
    f.turn("x", -90.0);
    // Turned about the fixed X axis afterwards: the three values still describe that turn.
    const fission::geo::Angles angles{f.value("rotX"), f.value("rotY"), f.value("rotZ")};
    const fission::geo::Matrix3 wanted =
        fission::geo::axisRotation(0, -90.0) * fission::geo::rotationOf({0.0, 0.0, 90.0});
    const fission::geo::Matrix3 got = fission::geo::rotationOf(angles);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            CHECK(got.m[r][c] == Approx(wanted.m[r][c]).margin(1e-9));
        }
    }
}

TEST_CASE("rotate: the body really turns, and keeps its volume", "[ui][rotate]") {
    Fixture f;
    // A flat plate 40 × 10 × 5 …
    REQUIRE(f.model->setStepValue(0, QStringLiteral("width"), QStringLiteral("40")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("depth"), QStringLiteral("10")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("height"), QStringLiteral("5")));
    f.refresh();
    auto box = f.extent();
    CHECK(box.maxX - box.minX == Approx(40.0).margin(0.01));
    // … turned a quarter round Z lies along Y …
    f.turn("z", 90.0);
    box = f.extent();
    CHECK(box.maxX - box.minX == Approx(10.0).margin(0.01));
    CHECK(box.maxY - box.minY == Approx(40.0).margin(0.01));
    // … and a quarter round X afterwards stands it up along Z.
    f.turn("x", 90.0);
    box = f.extent();
    CHECK(box.maxZ - box.minZ == Approx(40.0).margin(0.01));
    // The middle of the body stays where it was (it turns about its middle).
    CHECK((box.minX + box.maxX) / 2 == Approx(0.0).margin(0.01));
    CHECK((box.minY + box.maxY) / 2 == Approx(0.0).margin(0.01));
    CHECK((box.minZ + box.maxZ) / 2 == Approx(2.5).margin(0.01));
}

TEST_CASE("rotate: Esc puts the body back, and a drag in the air changes nothing", "[ui][rotate]") {
    Fixture f;
    const int before = f.stepsDone();
    f.turn("z", 60.0, /*release=*/false);
    CHECK(f.rings->editing());
    f.refresh();
    CHECK(f.value("rotZ") != Approx(0.0));
    CHECK(f.rings->turn()[2].toDouble() != 0.0);
    f.rings->cancelDrag();
    f.refresh();
    CHECK_FALSE(f.rings->editing());
    CHECK(f.value("rotZ") == Approx(0.0));
    CHECK(f.stepsDone() == before);

    // Taking a ring and letting go without moving is not a step.
    const QString z = QStringLiteral("z");
    const QPointF start = f.rings->ringPoint(z, f.rings->grabAngle(z));
    REQUIRE(f.rings->beginDrag(z, start.x(), start.y()));
    REQUIRE(f.rings->endDrag());
    CHECK(f.stepsDone() == before);
}

TEST_CASE("rotate: typing replaces dragging, Tab goes on to the next ring", "[ui][rotate]") {
    Fixture f;
    const int before = f.stepsDone();
    f.turn("z", 30.0, /*release=*/false);
    REQUIRE(f.rings->typeInsteadOfDrag());
    REQUIRE(f.rings->setTurn(2, 45.0)); // 45 degrees round Z, not the 30 the pointer made
    f.refresh();
    CHECK(f.value("rotZ") == Approx(45.0));
    REQUIRE(f.rings->setTurn(0, 90.0)); // Tab: and 90 round X
    REQUIRE(f.rings->commitEdit());
    f.refresh();
    CHECK(f.rings->turning().isEmpty());

    // Typed turns about several rings act in the order X, Y, Z (all about the axes of the bed).
    const fission::geo::Matrix3 wanted =
        fission::geo::axisRotation(2, 45.0) * fission::geo::axisRotation(0, 90.0);
    const fission::geo::Matrix3 got =
        fission::geo::rotationOf({f.value("rotX"), f.value("rotY"), f.value("rotZ")});
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            CHECK(got.m[r][c] == Approx(wanted.m[r][c]).margin(1e-9));
        }
    }
    CHECK(f.stepsDone() == before + 1); // the whole thing is one step
    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.value("rotX") == Approx(0.0));
    CHECK(f.value("rotZ") == Approx(0.0));
}

TEST_CASE("rotate: typing then Esc takes everything back", "[ui][rotate]") {
    Fixture f;
    f.turn("x", 15.0, /*release=*/false);
    REQUIRE(f.rings->typeInsteadOfDrag());
    REQUIRE(f.rings->setTurn(0, 120.0));
    f.refresh();
    CHECK(f.value("rotX") == Approx(120.0));
    f.rings->cancelEdit();
    f.refresh();
    CHECK(f.value("rotX") == Approx(0.0));
    CHECK_FALSE(f.rings->editing());
}

TEST_CASE("rotate: a body that is already turned keeps turning from where it is", "[ui][rotate]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotZ"), QStringLiteral("30")));
    f.refresh();
    f.turn("z", 60.0);
    // Started at 30; a ring turn of whole 15° steps ends at a multiple of 15, and not at 0.
    const double z = f.value("rotZ");
    CHECK(z != Approx(30.0));
    CHECK(std::fmod(std::abs(z - 30.0), 15.0) == Approx(0.0).margin(1e-9));
}

TEST_CASE("rotate: a ring seen edge on can still be turned", "[ui][rotate]") {
    Fixture f(0.0, 90.0, 300.0); // straight down: the X and Y rings are seen edge on
    const QString x = QStringLiteral("x");
    const QPointF start = f.rings->ringPoint(x, f.rings->grabAngle(x));
    REQUIRE(f.rings->beginDrag(x, start.x(), start.y()));
    CHECK(f.rings->dragTo(start.x(), start.y() - 60.0));
    CHECK(f.rings->endDrag());
    f.refresh();
    CHECK(std::abs(f.value("rotX")) >= 15.0);
}

TEST_CASE("rotate: the handles of the step turn with the body", "[ui][rotate]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotX"), QStringLiteral("90")));
    f.refresh();
    // The height handle of the box points along -Y now.
    bool found = false;
    for (const QVariant& entry : f.model->stepHandles()) {
        const QVariantMap handle = entry.toMap();
        if (handle["parameter"].toString() == QLatin1String("height")) {
            found = true;
            CHECK(handle["dy"].toDouble() == Approx(-1.0).margin(1e-9));
            CHECK(handle["dz"].toDouble() == Approx(0.0).margin(1e-9));
        }
    }
    CHECK(found);
}
