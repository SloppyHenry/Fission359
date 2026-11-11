// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The regions of a sketch through the controller (AP-2.1g): calculated in the background, kept
// until the new ones are there, stable ids after moving and after undo and redo, shown under the
// pointer, chosen by a click, let go by Esc or a click beside. No window: the clicks are lines of
// sight straight down onto the ground plane.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QString>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/sketch_controller.h"
#include "fission/ui/snap_session.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::SelectionController;
using fission::ui::SketchController;
namespace core = fission::core;
namespace interaction = fission::interaction;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct NoCatching {
    NoCatching() { fission::ui::setSnappingSuppressedForTests(true); }
    ~NoCatching() { fission::ui::setSnappingSuppressedForTests(false); }
};

struct Sketching {
    Sketching() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        selection = std::make_unique<SelectionController>(model.get());
        sketch = std::make_unique<SketchController>(model.get(), selection.get());
        sketch->start();
        REQUIRE(sketch->pickGround());
        model->waitForCalculation();
        QCoreApplication::processEvents();
        REQUIRE(sketch->planeReady());
    }

    static interaction::PlaneRay down(double u, double v) {
        return {{u, v, 100.0}, {0.0, 0.0, -1.0}};
    }
    void click(double u, double v) { sketch->clickAt(down(u, v), 2.0); }
    void move(double u, double v) { sketch->pointAt(down(u, v), 2.0); }

    // A 40 x 30 rectangle and a circle of radius 10 round the middle of its right side.
    void drawRectangleAndCircle() {
        sketch->chooseTool(QStringLiteral("rectangle"));
        click(0, 0);
        click(40, 30);
        sketch->chooseTool(QStringLiteral("circle"));
        click(40, 15);
        click(50, 15);
        sketch->chooseTool(QString());
    }
    bool regionsReady() { return sketch->waitForRegions(5000); }
    std::vector<std::string> ids() const {
        std::vector<std::string> out;
        for (const auto& info : sketch->regions().regions) {
            out.push_back(info.id);
        }
        std::ranges::sort(out);
        return out;
    }

    std::shared_ptr<fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    std::unique_ptr<SelectionController> selection;
    std::unique_ptr<SketchController> sketch;
};

} // namespace

TEST_CASE("sketch regions: a rectangle and a circle over its edge show three regions",
          "[ui][sketchregions]") {
    NoCatching free;
    Sketching f;
    CHECK(f.sketch->regionCount() == 0);
    f.drawRectangleAndCircle();
    REQUIRE(f.regionsReady());
    CHECK(f.sketch->regionCount() == 3);
    // the lens is half a circle: r = 10
    double smallest = 1e300;
    for (const auto& info : f.sketch->regions().regions) {
        smallest = std::min(smallest, info.shape.area);
    }
    CHECK(smallest == Approx(3.14159265358979 * 100.0 / 2.0).epsilon(1e-9));
    CHECK(f.sketch->regions().rejected == 0);
}

TEST_CASE("sketch regions: the old regions stay until the new ones are there",
          "[ui][sketchregions]") {
    NoCatching free;
    Sketching f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.click(40, 30);
    REQUIRE(f.regionsReady());
    REQUIRE(f.sketch->regionCount() == 1);
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(20, 15);
    f.click(25, 15); // the calculation is asked for now, its result comes with the next events
    CHECK(f.sketch->regionsBusy());
    CHECK(f.sketch->regionCount() == 1); // still the old one
    REQUIRE(f.regionsReady());
    CHECK(f.sketch->regionCount() == 2);
    CHECK_FALSE(f.sketch->regionsBusy());
}

TEST_CASE("sketch regions: ids stay when an element moves and after undo and redo",
          "[ui][sketchregions]") {
    NoCatching free;
    Sketching f;
    f.drawRectangleAndCircle();
    REQUIRE(f.regionsReady());
    const auto before = f.ids();
    REQUIRE(before.size() == 3);

    // the circle (the last element) moves a little to the left and gets smaller
    const auto elements = f.model->sketchElements(f.sketch->step());
    const std::string circle = elements.back().id;
    interaction::SketchEdit edit;
    edit.moves.push_back({circle, {{38, 14}, {44, 14}}});
    REQUIRE(f.model->applySketchEdit(f.sketch->step(), edit, QStringLiteral("trimSketch")));
    REQUIRE(f.regionsReady());
    CHECK(f.ids() == before);

    REQUIRE(f.model->undo());
    REQUIRE(f.regionsReady());
    CHECK(f.ids() == before);
    REQUIRE(f.model->redo());
    REQUIRE(f.regionsReady());
    CHECK(f.ids() == before);

    // taking the circle away leaves the rectangle's region with a new id
    interaction::SketchEdit remove;
    remove.removed.push_back(circle);
    REQUIRE(f.model->applySketchEdit(f.sketch->step(), remove, QStringLiteral("trimSketch")));
    REQUIRE(f.regionsReady());
    REQUIRE(f.sketch->regionCount() == 1);
    CHECK(std::ranges::find(before, f.ids().front()) == before.end());
}

TEST_CASE(
    "sketch regions: the pointer shows one, a click chooses it, Esc and a click beside let go",
    "[ui][sketchregions]") {
    NoCatching free;
    Sketching f;
    f.drawRectangleAndCircle();
    REQUIRE(f.regionsReady());
    CHECK(f.sketch->hoveredRegion().isEmpty());
    f.move(10, 15); // inside the rectangle only
    const QString rectangleOnly = f.sketch->hoveredRegion();
    CHECK_FALSE(rectangleOnly.isEmpty());
    f.move(40, 15); // in the lens
    CHECK_FALSE(f.sketch->hoveredRegion().isEmpty());
    CHECK(f.sketch->hoveredRegion() != rectangleOnly);
    f.move(-30, -30); // beside everything
    CHECK(f.sketch->hoveredRegion().isEmpty());

    CHECK(f.sketch->selectedRegion().isEmpty());
    f.click(10, 15);
    CHECK(f.sketch->selectedRegion() == rectangleOnly);
    f.click(48, 15); // the other part of the circle: the choice changes
    CHECK_FALSE(f.sketch->selectedRegion().isEmpty());
    CHECK(f.sketch->selectedRegion() != rectangleOnly);
    f.click(-30, -30); // a click beside lets go
    CHECK(f.sketch->selectedRegion().isEmpty());

    f.click(10, 15);
    REQUIRE_FALSE(f.sketch->selectedRegion().isEmpty());
    f.sketch->escape(); // lets go, the sketch stays
    CHECK(f.sketch->selectedRegion().isEmpty());
    CHECK(f.sketch->active());
    f.sketch->escape();
    CHECK_FALSE(f.sketch->active());
}

TEST_CASE("sketch regions: a click with a tool on draws and does not choose",
          "[ui][sketchregions]") {
    NoCatching free;
    Sketching f;
    f.drawRectangleAndCircle();
    REQUIRE(f.regionsReady());
    f.sketch->chooseTool(QStringLiteral("point"));
    f.click(10, 15);
    CHECK(f.sketch->selectedRegion().isEmpty());
    CHECK(f.model->sketchElements(f.sketch->step()).back().kind == core::SketchElementKind::point);
}

TEST_CASE("sketch regions: an open chain and helper lines give none; leaving forgets them",
          "[ui][sketchregions]") {
    NoCatching free;
    Sketching f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(40, 0);
    f.click(40, 30);
    f.sketch->escape();
    REQUIRE(f.regionsReady());
    CHECK(f.sketch->regionCount() == 0);
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.sketch->toggleHelperLine();
    f.click(0, 0);
    f.click(20, 20);
    REQUIRE(f.regionsReady());
    CHECK(f.sketch->regionCount() == 0); // a helper rectangle forms no area
    f.sketch->toggleHelperLine();
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.click(20, 20);
    REQUIRE(f.regionsReady());
    CHECK(f.sketch->regionCount() == 1);
    f.sketch->finish();
    CHECK(f.sketch->regionCount() == 0);
}

TEST_CASE("sketch regions: a chosen region is pulled into a body of 10 mm (AP-2.3b)",
          "[ui][sketchregions][extrude]") {
    NoCatching free;
    Sketching f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.click(40, 30);
    f.sketch->chooseTool(QString());
    REQUIRE(f.regionsReady());
    REQUIRE(f.ids().size() == 1);
    const QString region = QString::fromStdString(f.ids().front());
    f.sketch->finish();
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantList before = f.model->recipe();
    REQUIRE_FALSE(before.isEmpty());
    const QVariantMap sketchStep = before.back().toMap();
    REQUIRE(sketchStep.value(QStringLiteral("type")).toString() == QStringLiteral("sketch"));
    const QString sketchId = sketchStep.value(QStringLiteral("id")).toString();

    // An unknown region still makes the step; the step shows the error (never a guessed region).
    CHECK_FALSE(f.model->addExtrude(sketchId, QStringLiteral("nothing"), 20, 15).isEmpty());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().back().toMap().value(QStringLiteral("state")).toString() !=
          QStringLiteral("ok"));
    f.model->undo();
    CHECK(f.model->addExtrude(QStringLiteral("not-a-step"), region, 20, 15).isEmpty());

    const QString body = f.model->addExtrude(sketchId, region, 20, 15);
    REQUIRE_FALSE(body.isEmpty());
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantMap last = f.model->recipe().back().toMap();
    CHECK(last.value(QStringLiteral("type")).toString() == QStringLiteral("extrude"));
    CHECK(last.value(QStringLiteral("state")).toString() == QStringLiteral("ok"));
    CHECK(f.model->nextUndo().value(QStringLiteral("key")).toString() == QStringLiteral("extrude"));
}

namespace {

// The face called `name` of the first body (its number), 0 if there is none.
int faceNamed(DocumentModel& model, const QString& name) {
    const auto result = model.result();
    for (const auto& [id, body] : result->bodies) {
        for (const auto& face : body.faces) {
            if (face.name.ends_with(name.toStdString())) {
                return static_cast<int>(face.faceId);
            }
        }
    }
    return 0;
}

// The word "mode" of the step `stepId`, "" when it has none.
// The bodies listed on their own (not used up in a result).
int shownBodies(const DocumentModel& model) {
    int count = 0;
    for (const QVariant& body : model.bodies()) {
        count += body.toMap().value(QStringLiteral("consumed")).toBool() ? 0 : 1;
    }
    return count;
}

std::string modeOf(DocumentModel& model, const QString& stepId) {
    const core::Feature* step = model.document().findFeature(core::FeatureId{stepId.toStdString()});
    if (step == nullptr) {
        return {};
    }
    const auto mode = step->texts.find("mode");
    return mode != step->texts.end() ? mode->second : std::string();
}

} // namespace

TEST_CASE("sketch regions: pulled from a sketch on a face, it joins that body; pulled in, it "
          "cuts (AP-2.3b3)",
          "[ui][sketchregions][extrude]") {
    NoCatching free;
    Sketching f;
    f.sketch->finish(); // the ground sketch is empty: leaving it removes it
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    REQUIRE(f.model->addShape(QStringLiteral("box"), false) >= 0);
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const int bodiesBefore = shownBodies(*f.model);
    REQUIRE(bodiesBefore == 1);

    f.sketch->start();
    REQUIRE(f.sketch->pickFace(f.model->bodyIdAt(0), faceNamed(*f.model, "/box/top")));
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(-2, -2);
    f.click(2, 2);
    f.sketch->chooseTool(QString());
    REQUIRE(f.regionsReady());
    REQUIRE(f.ids().size() == 1);
    const QString region = QString::fromStdString(f.ids().front());
    f.sketch->finish();
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantMap sketchStep = f.model->recipe().back().toMap();
    REQUIRE(sketchStep.value(QStringLiteral("type")).toString() == QStringLiteral("sketch"));
    const QString sketchId = sketchStep.value(QStringLiteral("id")).toString();

    const QString body = f.model->addExtrude(sketchId, region, 0, 0);
    REQUIRE_FALSE(body.isEmpty());
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantList recipe = f.model->recipe();
    const QVariantMap last = recipe.back().toMap();
    REQUIRE(last.value(QStringLiteral("type")).toString() == QStringLiteral("extrude"));
    CHECK(last.value(QStringLiteral("state")).toString() == QStringLiteral("ok"));
    const QString lastId = last.value(QStringLiteral("id")).toString();
    const int lastIndex = f.model->stepIndexOfId(lastId);
    REQUIRE(lastIndex >= 0);
    CHECK(modeOf(*f.model, lastId) == "merge");
    CHECK(shownBodies(*f.model) == bodiesBefore);

    // Pulled into the body it cuts it: the mode follows the sign of the distance.
    REQUIRE(f.model->setStepValue(lastIndex, QStringLiteral("distance"), QStringLiteral("-1")));
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    CHECK(f.model->recipe().back().toMap().value(QStringLiteral("state")).toString() ==
          QStringLiteral("ok"));
    CHECK(modeOf(*f.model, lastId) == "subtract");
    CHECK(shownBodies(*f.model) == bodiesBefore);

    // One undo step brings the distance and the mode back.
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    CHECK(modeOf(*f.model, lastId) == "merge");
    f.model->selectStep(lastIndex);
    bool distanceSeen = false;
    for (const QVariant& entry : f.model->stepValues()) {
        const QVariantMap value = entry.toMap();
        if (value.value(QStringLiteral("name")).toString() == QStringLiteral("distance")) {
            CHECK(value.value(QStringLiteral("value")).toDouble() == Approx(10.0));
            distanceSeen = true;
        }
    }
    CHECK(distanceSeen);
}

TEST_CASE("sketch regions: pulled from the ground, a new body (AP-2.3b3)",
          "[ui][sketchregions][extrude]") {
    NoCatching free;
    Sketching f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.click(40, 30);
    f.sketch->chooseTool(QString());
    REQUIRE(f.regionsReady());
    REQUIRE(f.ids().size() == 1);
    const QString region = QString::fromStdString(f.ids().front());
    f.sketch->finish();
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantMap sketchStep = f.model->recipe().back().toMap();
    REQUIRE(sketchStep.value(QStringLiteral("type")).toString() == QStringLiteral("sketch"));
    const QString sketchId = sketchStep.value(QStringLiteral("id")).toString();
    const int bodiesBefore = shownBodies(*f.model);
    REQUIRE(bodiesBefore == 0);

    const QString body = f.model->addExtrude(sketchId, region, 20, 15);
    REQUIRE_FALSE(body.isEmpty());
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantMap last = f.model->recipe().back().toMap();
    CHECK(last.value(QStringLiteral("type")).toString() == QStringLiteral("extrude"));
    CHECK(last.value(QStringLiteral("state")).toString() == QStringLiteral("ok"));
    CHECK(modeOf(*f.model, last.value(QStringLiteral("id")).toString()) == "new");
    CHECK(shownBodies(*f.model) == bodiesBefore + 1);
}

TEST_CASE("sketch regions: deleting a sketch that is pulled names the pull as needing help "
          "(AP-2.3b3)",
          "[ui][sketchregions][extrude]") {
    NoCatching free;
    Sketching f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.click(40, 30);
    f.sketch->chooseTool(QString());
    REQUIRE(f.regionsReady());
    REQUIRE(f.ids().size() == 1);
    const QString region = QString::fromStdString(f.ids().front());
    f.sketch->finish();
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    int sketchIndex = -1;
    const QVariantList recipe = f.model->recipe();
    for (int i = 0; i < recipe.size(); ++i) {
        const QVariantMap entry = recipe.at(i).toMap();
        if (entry.value(QStringLiteral("type")).toString() == QStringLiteral("sketch")) {
            sketchIndex = i;
        }
    }
    REQUIRE(sketchIndex >= 0);
    const QString sketchId = recipe.at(sketchIndex).toMap().value(QStringLiteral("id")).toString();

    const QString body = f.model->addExtrude(sketchId, region, 20, 15);
    REQUIRE_FALSE(body.isEmpty());
    f.model->waitForCalculation();
    QCoreApplication::processEvents();
    const QVariantList after = f.model->recipe();
    REQUIRE(after.size() == sketchIndex + 2);
    const int extrudeIndex = static_cast<int>(after.size()) - 1;
    CHECK(after.back().toMap().value(QStringLiteral("type")).toString() ==
          QStringLiteral("extrude"));

    const QVariantMap plan = f.model->deletionOfStep(sketchIndex);
    CHECK(plan.value(QStringLiteral("dependents")).toList().contains(QVariant(extrudeIndex)));
}
