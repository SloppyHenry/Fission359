// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The tool "Measure" and the facts about a body in the interface (AP-2.11). The 3D view is not
// drawn here; its answers are sent by hand, like in the tests of the selection.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/measure_controller.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::core::SelectableKind;
using fission::ui::DocumentModel;
using fission::ui::MeasureController;
using fission::ui::SelectionController;
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
    SelectionController selection{&model};
    MeasureController measure{&model, &selection, kernel};
    Viewport3D viewport;

    World() {
        application();
        model.addDemoBox(); // 20 x 20 x 20 mm
        model.waitForCalculation();
        selection.attach(&viewport);
        measure.attach(&viewport);
        viewport.setScene(model.scene());
    }

    QString body() const { return model.bodyIdAt(0); }

    void click(SelectableKind kind, int number, bool extend = false) {
        emit viewport.picked(static_cast<int>(Viewport3D::PickIntent::click), extend, body(),
                             static_cast<int>(kind), number);
    }
    int faceNumber(const std::string& name) const {
        const auto names = model.elementNames();
        return static_cast<int>(
            names->at(fission::core::BodyId{body().toStdString()}).findFace(name).value());
    }
};

} // namespace

TEST_CASE("measure: nothing is measured while the tool is off", "[measure-ui]") {
    World world;
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    CHECK(world.measure.result().isEmpty());
    CHECK_FALSE(world.measure.enabled());
}

TEST_CASE("measure: one face gives its area, two give distance and angle", "[measure-ui]") {
    World world;
    world.measure.setEnabled(true);
    CHECK(world.measure.result().isEmpty()); // nothing chosen yet

    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    QVariantMap one = world.measure.result();
    CHECK(one.value("count").toInt() == 1);
    CHECK(one.value("first").toString() == QStringLiteral("face"));
    CHECK(one.value("area").toDouble() == Approx(400.0).epsilon(1e-6));
    CHECK_FALSE(one.contains("distance"));

    world.click(SelectableKind::face, world.faceNumber("F1/box/bottom"), /*extend=*/true);
    const QVariantMap two = world.measure.result();
    CHECK(two.value("count").toInt() == 2);
    CHECK(two.value("distance").toDouble() == Approx(20.0).epsilon(1e-6));
    CHECK(two.value("angle").toDouble() == Approx(0.0).margin(1e-6)); // parallel
}

TEST_CASE("measure: the last two chosen are measured", "[measure-ui]") {
    World world;
    world.measure.setEnabled(true);
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    world.click(SelectableKind::face, world.faceNumber("F1/box/bottom"), true);
    world.click(SelectableKind::face, world.faceNumber("F1/box/front"), true);
    const QVariantMap result = world.measure.result();
    CHECK(result.value("count").toInt() == 2);
    // bottom and front meet at an edge: inside angle 90.
    CHECK(result.value("angle").toDouble() == Approx(90.0).epsilon(1e-6));
}

TEST_CASE("measure: a body as a whole is nothing to measure here", "[measure-ui]") {
    World world;
    world.measure.setEnabled(true);
    world.selection.chooseBody(0, false);
    CHECK(world.measure.result().isEmpty());
    world.measure.setEnabled(false);
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    CHECK(world.measure.result().isEmpty());
}

TEST_CASE("measure: the facts about a body", "[measure-ui]") {
    World world;
    const QVariantMap facts = world.measure.bodyFacts(world.body());
    REQUIRE(facts.value("valid").toBool());
    CHECK(facts.value("volume").toDouble() == Approx(8000.0).epsilon(1e-6));
    CHECK(facts.value("area").toDouble() == Approx(2400.0).epsilon(1e-6));
    CHECK(facts.value("sizeX").toDouble() == Approx(20.0).epsilon(1e-6));
    CHECK(facts.value("sizeZ").toDouble() == Approx(20.0).epsilon(1e-6));
    CHECK_FALSE(world.measure.bodyFacts(QStringLiteral("no-such-body")).value("valid").toBool());
    CHECK_FALSE(world.measure.bodyFacts(QString()).value("valid").toBool());
}

TEST_CASE("measure: weight and filament of the cube in PLA", "[measure-ui]") {
    World world;
    const QVariantList materials = world.measure.materials();
    REQUIRE(materials.size() == 5);
    CHECK(materials.front().toMap().value("id").toString() == QStringLiteral("pla"));
    const double density = materials.front().toMap().value("density").toDouble();
    const QVariantMap weight = world.measure.weightOf(8000.0, density, 1.75);
    CHECK(weight.value("grams").toDouble() == Approx(9.92).epsilon(1e-9));
    CHECK(weight.value("filamentMetres").toDouble() == Approx(3.326).margin(0.001));
}

TEST_CASE("measure: the line of a measure is drawn in the view and goes away", "[measure-ui]") {
    World world;
    world.measure.setEnabled(true);
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    world.click(SelectableKind::face, world.faceNumber("F1/box/bottom"), true);
    const auto& graphics = world.viewport.handleGraphics(Viewport3D::HandleSource::measure);
    CHECK_FALSE(graphics.lines.empty());
    world.measure.setEnabled(false);
    CHECK(world.viewport.handleGraphics(Viewport3D::HandleSource::measure).lines.empty());
}
