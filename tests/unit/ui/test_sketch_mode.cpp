// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The sketch mode (AP-2.1b): choosing the plane (ground or a flat face), the step in the recipe,
// the pale bodies, leaving it (an empty sketch goes, undo brings it back), opening a sketch
// again, and the pointer on the plane. No window: the controller works without a view.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QStringList>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/sketch_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::SelectionController;
using fission::ui::SketchController;
using fission::ui::Viewport3D;
namespace core = fission::core;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// The notices a controller sends (no QtTest in this build).
struct Notices {
    explicit Notices(const SketchController& controller) {
        QObject::connect(&controller, &SketchController::notice, &holder,
                         [this](const QString& code) { codes << code; });
    }
    QObject holder;
    QStringList codes;
};

struct Sketching {
    Sketching() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        selection = std::make_unique<SelectionController>(model.get());
        sketch = std::make_unique<SketchController>(model.get(), selection.get());
    }

    int steps() const { return static_cast<int>(model->recipe().size()); }
    QString typeAt(int index) const {
        return model->recipe().at(index).toMap().value("type").toString();
    }
    bool activeAt(int index) const {
        return model->recipe().at(index).toMap().value("active").toBool();
    }

    // Lets the calculation finish and the controller see the result.
    void settle() {
        model->waitForCalculation();
        QCoreApplication::processEvents();
    }

    // The number of the face of the first body that has this name ("F1/box/top").
    int faceNamed(const QString& name) const {
        const auto result = model->result();
        REQUIRE(result != nullptr);
        for (const auto& [id, body] : result->bodies) {
            for (const core::BodyFace& face : body.faces) {
                if (face.name == name.toStdString()) {
                    return static_cast<int>(face.faceId);
                }
            }
        }
        FAIL("no face named " << name.toStdString());
        return 0;
    }

    QString firstBody() const { return model->bodyIdAt(0); }

    std::shared_ptr<fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    std::unique_ptr<SelectionController> selection;
    std::unique_ptr<SketchController> sketch;
};

} // namespace

TEST_CASE("sketch mode: the button starts choosing, the ground plane makes a sketch step",
          "[ui][sketchmode]") {
    Sketching f;
    CHECK_FALSE(f.sketch->engaged());
    f.sketch->start();
    CHECK(f.sketch->picking());
    CHECK(f.steps() == 0); // nothing yet: the plane is not chosen

    REQUIRE(f.sketch->pickGround());
    f.settle();
    CHECK(f.sketch->active());
    CHECK_FALSE(f.sketch->picking());
    REQUIRE(f.steps() == 1);
    CHECK(f.typeAt(0) == "sketch");
    CHECK(f.activeAt(0)); // the recipe marks the sketch being drawn in
    CHECK(f.sketch->planeReady());
    CHECK(f.sketch->gridStep() > 0.0);

    const auto result = f.model->sketchResult(f.sketch->step());
    REQUIRE(result.has_value());
    REQUIRE(result->plane.has_value());
    CHECK(result->plane->normal().z == Approx(1.0));
    CHECK(result->plane->origin().z == Approx(0.0)); // the bed, z = 0
}

TEST_CASE("sketch mode: Esc cancels choosing, and nothing was made", "[ui][sketchmode]") {
    Sketching f;
    f.sketch->start();
    f.sketch->escape();
    CHECK_FALSE(f.sketch->engaged());
    CHECK(f.steps() == 0);
}

TEST_CASE("sketch mode: an empty sketch is removed on Done, and undo brings it back",
          "[ui][sketchmode]") {
    Sketching f;
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    REQUIRE(f.steps() == 1);

    f.sketch->finish();
    f.settle();
    CHECK_FALSE(f.sketch->engaged());
    CHECK(f.steps() == 0);
    CHECK(f.model->nextUndo().value("key").toString() == "removeFeature");

    // Undo puts the sketch back (the same step), redo takes it away again, one step each.
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.steps() == 1);
    CHECK(f.typeAt(0) == "sketch");
    CHECK(f.model->nextUndo().value("key").toString() == "addFeature");
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.steps() == 0);
    // Undoing the first step too leaves the document as it was before the sketch.
    REQUIRE(f.model->undo());
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.steps() == 0);
    CHECK(f.model->bodies().isEmpty());
}

TEST_CASE("sketch mode: a sketch with something in it stays when it is left", "[ui][sketchmode]") {
    Sketching f;
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    const QString id = f.sketch->step();

    // Something drawn in it (the drawing tools come with AP-2.1c): a line, by the command.
    core::Document document = f.model->document();
    REQUIRE(document.addSketchElement(core::FeatureId{id.toStdString()},
                                      core::SketchElementKind::line, {{0.0, 0.0}, {30.0, 0.0}}));
    f.sketch->finish();
    f.model->replaceDocument(document);
    f.settle();
    CHECK(f.steps() == 1);
    CHECK(f.typeAt(0) == "sketch");
}

TEST_CASE("sketch mode: the other bodies are pale while sketching and solid again after",
          "[ui][sketchmode]") {
    Sketching f;
    REQUIRE(f.model->addShape("box", false) >= 0);
    f.settle();
    REQUIRE(f.model->scene()->bodies.size() == 1);
    CHECK(f.model->scene()->bodies[0].color.alpha() == 255);

    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    REQUIRE(f.model->scene()->bodies.size() == 1);
    const int alpha = f.model->scene()->bodies[0].color.alpha();
    CHECK(alpha > 70);
    CHECK(alpha < 110); // about 35 %

    f.sketch->finish();
    f.settle();
    CHECK(f.model->scene()->bodies[0].color.alpha() == 255);
}

TEST_CASE("sketch mode: a click on the top of a box makes a sketch on it that follows the box",
          "[ui][sketchmode]") {
    Sketching f;
    REQUIRE(f.model->addShape("box", false) >= 0);
    f.settle();
    const int top = f.faceNamed("F1/box/top");

    f.sketch->start();
    REQUIRE(f.sketch->pickFace(f.firstBody(), top));
    f.settle();
    REQUIRE(f.steps() == 2);
    CHECK(f.typeAt(1) == "sketch");
    CHECK(f.sketch->planeReady());

    const core::Feature* step =
        f.model->document().findFeature(core::FeatureId{f.sketch->step().toStdString()});
    REQUIRE(step != nullptr);
    CHECK(step->inputs.size() == 1);
    CHECK(step->refs.contains("plane"));
    // The box is still a whole body of its own (handles, selection): the sketch only reads it.
    CHECK_FALSE(f.model->document().consumerOf(step->inputs.front()).has_value());

    auto plane = f.model->sketchResult(f.sketch->step())->plane;
    REQUIRE(plane.has_value());
    CHECK(plane->normal().z == Approx(1.0));
    const double height = plane->origin().z;
    CHECK(height > 1.0);

    // The box moves: the plane moves with it.
    REQUIRE(f.model->setStepValue(0, QStringLiteral("x"), QStringLiteral("50")));
    f.settle();
    plane = f.model->sketchResult(f.sketch->step())->plane;
    REQUIRE(plane.has_value());
    CHECK(plane->origin().x == Approx(50.0).margin(1e-9));
    CHECK(plane->origin().z == Approx(height));
}

TEST_CASE("sketch mode: a click on a curved face makes no step and says why", "[ui][sketchmode]") {
    Sketching f;
    REQUIRE(f.model->addShape("cylinder", false) >= 0);
    f.settle();
    const int side = f.faceNamed("F1/cylinder/side");
    CHECK_FALSE(f.model->faceIsFlat(f.firstBody(), side));

    Notices notices(*f.sketch);
    f.sketch->start();
    CHECK_FALSE(f.sketch->pickFace(f.firstBody(), side));
    REQUIRE(notices.codes.size() == 1);
    CHECK(notices.codes.front() == "curvedFace");
    CHECK(f.steps() == 1);      // only the cylinder
    CHECK(f.sketch->picking()); // still waiting for a better click
    // The flat top of the same cylinder works.
    CHECK(f.sketch->pickFace(f.firstBody(), f.faceNamed("F1/cylinder/top")));
    CHECK(f.steps() == 2);
}

TEST_CASE("sketch mode: a sketch whose face is gone needs help and is not opened",
          "[ui][sketchmode]") {
    Sketching f;
    REQUIRE(f.model->addShape("box", false) >= 0);
    f.settle();
    f.sketch->start();
    REQUIRE(f.sketch->pickFace(f.firstBody(), f.faceNamed("F1/box/top")));
    f.settle();
    const QString id = f.sketch->step();

    // Draw something so that the sketch stays, leave, then take the body away from under it.
    core::Document document = f.model->document();
    REQUIRE(document.addSketchElement(core::FeatureId{id.toStdString()},
                                      core::SketchElementKind::point, {{1.0, 1.0}}));
    f.sketch->finish();
    f.model->replaceDocument(document);
    f.settle();
    CHECK(f.sketch->reopen(1)); // it opens again while its face is there
    CHECK(f.sketch->active());
    f.sketch->finish();

    // The sketch step without its body: it cannot find its face, it is not calculated.
    core::Document broken = f.model->document();
    const core::Feature* step = broken.findFeature(core::FeatureId{id.toStdString()});
    REQUIRE(step != nullptr);
    REQUIRE(broken.setFeatureInputs(step->id, {}));
    f.model->replaceDocument(broken);
    f.settle();
    Notices notices(*f.sketch);
    CHECK_FALSE(f.sketch->reopen(1));
    REQUIRE(notices.codes.size() == 1);
    CHECK(notices.codes.front() == "needsHelp");
    CHECK_FALSE(f.sketch->engaged());
    CHECK(f.model->recipe().at(1).toMap().value("state").toString() == "warning");
}

TEST_CASE("sketch mode: undoing the sketch while in it leaves the mode", "[ui][sketchmode]") {
    Sketching f;
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    REQUIRE(f.sketch->active());
    REQUIRE(f.model->undo());
    f.settle();
    CHECK_FALSE(f.sketch->engaged());
    CHECK(f.steps() == 0);
    CHECK(f.model->activeSketch().isEmpty());
}

TEST_CASE("sketch mode: the pointer on the plane catches on the grid and shows a marker",
          "[ui][sketchmode]") {
    Sketching f;
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();

    // 2 px per mm: the grid is 5 mm (lines at least 6 pixels apart).
    f.sketch->pointAt({{12.4, -7.6, 100.0}, {0.0, 0.0, -1.0}}, 2.0);
    QVariantMap snap = f.sketch->snap();
    REQUIRE(snap.value("valid").toBool());
    CHECK(snap.value("kind").toString() == "grid");
    CHECK(snap.value("u").toDouble() == Approx(10.0));
    CHECK(snap.value("v").toDouble() == Approx(-10.0));
    CHECK(f.sketch->gridStep() == Approx(5.0));

    // Zoomed in the grid is finer, 1 mm.
    f.sketch->pointAt({{12.4, -7.6, 100.0}, {0.0, 0.0, -1.0}}, 10.0);
    snap = f.sketch->snap();
    CHECK(snap.value("u").toDouble() == Approx(12.0));
    CHECK(snap.value("v").toDouble() == Approx(-8.0));

    // A ray that runs along the plane meets nothing: no marker.
    f.sketch->pointAt({{0.0, 0.0, 5.0}, {1.0, 0.0, 0.0}}, 2.0);
    CHECK_FALSE(f.sketch->snap().value("valid").toBool());
}

TEST_CASE("sketch mode: the pointer catches on the elements of the sketch", "[ui][sketchmode]") {
    Sketching f;
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    // A line in the sketch (the drawing tools come with AP-2.1c): added to a copy of the
    // document, which replaces the open one.
    core::Document document = f.model->document();
    REQUIRE(document.addSketchElement(core::FeatureId{f.sketch->step().toStdString()},
                                      core::SketchElementKind::line, {{0.0, 0.0}, {40.0, 20.0}}));
    f.sketch->finish();
    f.model->replaceDocument(document);
    f.settle();
    REQUIRE(f.sketch->reopen(0));
    f.settle();
    REQUIRE(f.sketch->planeReady());

    // At 2 px/mm the reach is 5 mm: near the end of the line it catches on it.
    f.sketch->pointAt({{41.0, 19.0, 100.0}, {0.0, 0.0, -1.0}}, 2.0);
    QVariantMap snap = f.sketch->snap();
    CHECK(snap.value("kind").toString() == "point");
    CHECK(snap.value("u").toDouble() == Approx(40.0));
    CHECK(snap.value("v").toDouble() == Approx(20.0));
    CHECK(snap.value("element").toString() == "L1");

    // Near the middle of the line: the middle.
    f.sketch->pointAt({{21.5, 11.0, 100.0}, {0.0, 0.0, -1.0}}, 2.0);
    snap = f.sketch->snap();
    CHECK(snap.value("kind").toString() == "middle");
    CHECK(snap.value("u").toDouble() == Approx(20.0));
    CHECK(snap.value("v").toDouble() == Approx(10.0));

    // Level with the end of the line, far from everything else: lines up, with a guide.
    f.sketch->pointAt({{-30.0, 21.0, 100.0}, {0.0, 0.0, -1.0}}, 2.0);
    snap = f.sketch->snap();
    CHECK(snap.value("kind").toString() == "alignment");
    CHECK(snap.value("v").toDouble() == Approx(20.0));
    CHECK(snap.value("guides").toInt() == 1);
}

TEST_CASE("sketch mode: a click into the empty (the answer of the pick pass) chooses the ground",
          "[ui][sketchmode]") {
    Sketching f;
    Viewport3D view;
    view.setProperty("animated", false);
    view.setWidth(700.0);
    view.setHeight(700.0);
    f.selection->attach(&view);
    f.sketch->attach(&view);
    f.model->addShape("box", false);
    f.settle();

    f.sketch->start();
    REQUIRE(f.sketch->picking());
    // What the view reports for a click on nothing: no body, kind "body", number 0.
    emit view.picked(static_cast<int>(Viewport3D::PickIntent::click), false, QString(), 0, 0);
    f.settle();
    CHECK(f.sketch->active());
    REQUIRE(f.steps() == 2);
    CHECK(f.typeAt(1) == "sketch");
    // The way of the window test: Done, undo, redo, and the ground again.
    f.sketch->finish();
    f.settle();
    CHECK(f.steps() == 1);
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.steps() == 2);
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.steps() == 1);
    f.sketch->start();
    REQUIRE(f.sketch->picking());
    emit view.picked(static_cast<int>(Viewport3D::PickIntent::click), false, QString(), 0, 0);
    f.settle();
    CHECK(f.sketch->active());
    f.sketch->escape();
    f.settle();
    CHECK_FALSE(f.sketch->engaged());
    // A hover is not a click.
    f.sketch->start();
    emit view.picked(static_cast<int>(Viewport3D::PickIntent::hover), false, QString(), 0, 0);
    CHECK(f.sketch->picking());
}
