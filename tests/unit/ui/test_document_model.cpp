// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <atomic>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/numbers.h"

using Catch::Approx;
using fission::ui::DocumentModel;

namespace {

// The model delivers its results through the event loop of the interface thread.
QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct Extent {
    double minX = 1e300;
    double maxX = -1e300;
    double minZ = 1e300;
    double maxZ = -1e300;
};

Extent extentOf(const fission::render::RenderScene& scene) {
    Extent extent;
    for (const auto& body : scene.bodies) {
        for (const auto& point : body.geometry().positions) {
            extent.minX = std::min(extent.minX, static_cast<double>(point.x));
            extent.maxX = std::max(extent.maxX, static_cast<double>(point.x));
            extent.minZ = std::min(extent.minZ, static_cast<double>(point.z));
            extent.maxZ = std::max(extent.maxZ, static_cast<double>(point.z));
        }
    }
    return extent;
}

// Counts how often a body was really built: undo should come from the cache.
class CountingKernel final : public fission::geo::GeometryKernel {
public:
    explicit CountingKernel(std::unique_ptr<fission::geo::GeometryKernel> inner)
        : inner_(std::move(inner)) {}

    fission::geo::NamedSolid makeBox(double x, double y, double z, fission::geo::Point3d corner,
                                     double cornerRadius) const override {
        ++builds;
        return inner_->makeBox(x, y, z, corner, cornerRadius);
    }
    fission::geo::NamedSolid makePrimitive(const fission::geo::PrimitiveSpec& spec,
                                           fission::geo::Point3d base) const override {
        ++builds;
        return inner_->makePrimitive(spec, base);
    }
    fission::geo::NamedSolid makeCylinder(double radius, double height,
                                          fission::geo::Point3d base) const override {
        ++builds;
        return inner_->makeCylinder(radius, height, base);
    }
    fission::geo::NamedSolid
    boolean(fission::geo::BooleanOp op, const fission::geo::NamedSolid& target,
            const std::vector<fission::geo::NamedSolid>& tools) const override {
        return inner_->boolean(op, target, tools);
    }
    fission::geo::SolidReport measure(const fission::geo::Solid& solid) const override {
        return inner_->measure(solid);
    }
    fission::geo::Bounds bounds(const fission::geo::Solid& solid) const override {
        return inner_->bounds(solid);
    }
    fission::geo::TriangleMesh
    tessellate(const fission::geo::Solid& solid,
               const fission::geo::TessellationParams& p) const override {
        double finest = finestChord.load();
        while (p.chordTolerance < finest &&
               !finestChord.compare_exchange_weak(finest, p.chordTolerance)) {
        }
        return inner_->tessellate(solid, p);
    }

    mutable std::atomic<int> builds{0};
    mutable std::atomic<double> finestChord{1.0}; ///< the finest tolerance asked for so far

private:
    std::unique_ptr<fission::geo::GeometryKernel> inner_;
};

struct Fixture {
    Fixture() {
        application();
        auto counting = std::make_shared<CountingKernel>(fission::geo::occt::makeKernel());
        counter = counting.get();
        std::shared_ptr<const fission::geo::GeometryKernel> kernel = counting;
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox();
        model->waitForCalculation();
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }

    std::unique_ptr<DocumentModel> model;
    CountingKernel* counter = nullptr;
};

} // namespace

TEST_CASE("the box of the start is a real step of the document", "[ui][document-model]") {
    Fixture f;
    REQUIRE(f.model->recipe().size() == 1);
    const QVariantMap step = f.step(0);
    CHECK(step["type"].toString() == "box");
    CHECK(step["state"].toString() == "ok");
    const QVariantList values = step["values"].toList();
    REQUIRE(values.size() == 3);
    CHECK(values[0].toDouble() == Approx(20.0));

    REQUIRE(f.model->bodies().size() == 1);
    const QVariantMap body = f.model->bodies().at(0).toMap();
    CHECK(body["type"].toString() == "box");
    CHECK(body["number"].toInt() == 1);
    CHECK(body["visible"].toBool());

    // The scene shows it: 20 mm wide, standing on the bed.
    REQUIRE(f.model->scene() != nullptr);
    REQUIRE(f.model->scene()->bodies.size() == 1);
    const Extent extent = extentOf(*f.model->scene());
    CHECK(extent.minX == Approx(-10.0));
    CHECK(extent.maxX == Approx(10.0));
    CHECK(extent.minZ == Approx(0.0).margin(1e-6));
    CHECK(extent.maxZ == Approx(20.0));
}

TEST_CASE("acceptance: a changed value moves the body", "[ui][document-model][acceptance]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, "width", "35"));
    f.model->waitForCalculation();

    const Extent extent = extentOf(*f.model->scene());
    CHECK(extent.minX == Approx(-17.5));
    CHECK(extent.maxX == Approx(17.5));
    CHECK(f.step(0)["values"].toList()[0].toDouble() == Approx(35.0));
    CHECK(f.step(0)["state"].toString() == "ok");

    // Height too: it grows upwards from the bed, it is not scaled around the middle.
    REQUIRE(f.model->setStepValue(0, "height", "2 cm + 5"));
    f.model->waitForCalculation();
    const Extent taller = extentOf(*f.model->scene());
    CHECK(taller.minZ == Approx(0.0).margin(1e-6));
    CHECK(taller.maxZ == Approx(25.0));
    CHECK(taller.maxX == Approx(17.5)); // width untouched
}

TEST_CASE("values of the chosen step, formulas stay visible", "[ui][document-model]") {
    Fixture f;
    CHECK(f.model->currentStep() == -1);
    CHECK(f.model->stepValues().isEmpty());

    f.model->selectStep(0);
    CHECK(f.model->currentStep() == 0);
    QVariantList values = f.model->stepValues();
    REQUIRE(values.size() == 11); // width depth height cornerRadius x y z rotX rotY rotZ through
    CHECK(values[0].toMap()["name"].toString() == "width");
    CHECK(values[0].toMap()["group"].toString() == "size");
    CHECK(values[0].toMap()["expression"].toString().isEmpty()); // typed as a plain number
    CHECK(values[3].toMap()["name"].toString() == "cornerRadius");
    CHECK(values[4].toMap()["group"].toString() == "position");
    CHECK(values[4].toMap()["axis"].toString() == "x");
    CHECK(values[7].toMap()["name"].toString() == "rotX");
    CHECK(values[7].toMap()["group"].toString() == "rotation");
    CHECK(values[7].toMap()["axis"].toString() == "x");
    CHECK(values[7].toMap()["kind"].toString() == "angle");
    CHECK(values[9].toMap()["axis"].toString() == "z");

    f.model->setStepValue(0, "depth", "10 + 5");
    f.model->waitForCalculation();
    const QVariantMap depth = f.model->stepValues().at(1).toMap();
    CHECK(depth["expression"].toString() == "10 + 5"); // the formula, not just 15
    CHECK(depth["value"].toDouble() == Approx(15.0));

    f.model->selectStep(7); // there is no such step
    CHECK(f.model->currentStep() == -1);
}

TEST_CASE("a value that cannot be calculated is an error on the step, the body stays",
          "[ui][document-model]") {
    Fixture f;
    f.model->setStepValue(0, "width", "Breitee");
    f.model->waitForCalculation();

    const QVariantMap step = f.step(0);
    CHECK(step["state"].toString() == "fails");
    const QVariantMap problem = step["problem"].toMap();
    CHECK(problem["code"].toString() == "parameterInvalid");
    CHECK(problem["field"].toString() == "width");
    CHECK(problem["detail"].toString() == "unknownName");
    CHECK(problem["subject"].toString() == "Breitee");

    // The last valid shape is still shown (and marked in the core as stale).
    REQUIRE(f.model->scene()->bodies.size() == 1);
    CHECK(extentOf(*f.model->scene()).maxX == Approx(10.0));

    f.model->setStepValue(0, "width", "30");
    f.model->waitForCalculation();
    CHECK(f.step(0)["state"].toString() == "ok");
    CHECK(f.step(0)["problem"].isNull());
}

TEST_CASE("hiding a body does not calculate again", "[ui][document-model]") {
    Fixture f;
    const auto revision = f.model->result()->revision;
    f.model->setBodyVisible(0, false);
    CHECK(f.model->scene()->bodies.empty());
    CHECK_FALSE(f.model->bodies().at(0).toMap()["visible"].toBool());
    CHECK_FALSE(f.model->calculating());
    CHECK(f.model->result()->revision == revision);
    f.model->setBodyVisible(0, true);
    CHECK(f.model->scene()->bodies.size() == 1);
}

TEST_CASE("number fields know the named measures of the document", "[ui][numbers]") {
    fission::ui::Numbers numbers;
    CHECK(std::isnan(numbers.evaluate("Breite * 2")));
    numbers.setNamedMeasures({{"Breite", 25.0}});
    CHECK(numbers.evaluate("Breite * 2") == Approx(50.0));
    CHECK(numbers.evaluate("breite + 1 cm") == Approx(35.0));
}

TEST_CASE("undo and redo bring the old shape back, without building it again",
          "[ui][document-model][history]") {
    Fixture f;
    CHECK_FALSE(f.model->canUndo()); // the box of the start is not something the person did
    CHECK_FALSE(f.model->canRedo());

    REQUIRE(f.model->setStepValue(0, "width", "35"));
    f.model->waitForCalculation();
    CHECK(f.model->canUndo());
    CHECK(extentOf(*f.model->scene()).maxX == Approx(17.5));
    const int builds = f.counter->builds;

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(extentOf(*f.model->scene()).maxX == Approx(10.0));
    CHECK(f.step(0)["values"].toList()[0].toDouble() == Approx(20.0));
    CHECK_FALSE(f.model->canUndo());
    CHECK(f.model->canRedo());

    REQUIRE(f.model->redo());
    f.model->waitForCalculation();
    CHECK(extentOf(*f.model->scene()).maxX == Approx(17.5));
    CHECK(f.counter->builds == builds); // both came from the cache
}

TEST_CASE("every change of the interface is a step, and says what it was",
          "[ui][document-model][history]") {
    Fixture f;
    f.model->setStepValue(0, "width", "35");
    f.model->setBodyVisible(0, false);
    f.model->waitForCalculation();

    const QVariantList history = f.model->history();
    REQUIRE(history.size() == 2);
    const QVariantMap first = history.at(0).toMap();
    CHECK(first["key"].toString() == "setValue");
    CHECK(first["field"].toString() == "width");
    CHECK(first["value"].toString() == "35");
    CHECK(first["type"].toString() == "box");
    CHECK(first["done"].toBool());
    CHECK(history.at(1).toMap()["key"].toString() == "hideBody");
    CHECK(f.model->nextUndo()["key"].toString() == "hideBody");

    // Undoing the hiding shows the body; the list keeps it as something that can be redone.
    REQUIRE(f.model->undo());
    CHECK(f.model->scene()->bodies.size() == 1);
    CHECK(f.model->historyPosition() == 1);
    CHECK_FALSE(f.model->history().at(1).toMap()["done"].toBool());
    CHECK(f.model->nextRedo()["key"].toString() == "hideBody");
}

TEST_CASE("a value typed again that did not change is no step", "[ui][document-model][history]") {
    Fixture f;
    CHECK(f.model->setStepValue(0, "width", "20")); // it is 20 already
    CHECK_FALSE(f.model->canUndo());
    CHECK(f.model->history().isEmpty());
    CHECK_FALSE(f.model->setStepValue(0, "nonsense", "3")); // refused, no step either
    CHECK_FALSE(f.model->canUndo());
}

TEST_CASE("a drag is one step", "[ui][document-model][history]") {
    Fixture f;
    f.model->beginChange("width");
    for (int width = 21; width <= 60; ++width) {
        REQUIRE(f.model->setStepValue(0, "width", QString::number(width)));
    }
    f.model->commitChange();
    f.model->waitForCalculation();

    CHECK(f.model->history().size() == 1);
    CHECK(extentOf(*f.model->scene()).maxX == Approx(30.0));
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(extentOf(*f.model->scene()).maxX == Approx(10.0));
    CHECK_FALSE(f.model->canUndo());

    // Esc during a drag: back to where it started, no step.
    f.model->beginChange("width");
    f.model->setStepValue(0, "width", "99");
    f.model->cancelChange();
    f.model->waitForCalculation();
    CHECK(f.step(0)["values"].toList()[0].toDouble() == Approx(20.0));
    CHECK(f.model->history().size() == 1); // only the undone drag is left to redo
}

TEST_CASE("going to a step of the list", "[ui][document-model][history]") {
    Fixture f;
    f.model->setStepValue(0, "width", "30");
    f.model->setStepValue(0, "depth", "40");
    f.model->setStepValue(0, "height", "50");
    f.model->waitForCalculation();

    REQUIRE(f.model->goToHistory(1));
    f.model->waitForCalculation();
    const QVariantList values = f.step(0)["values"].toList();
    CHECK(values[0].toDouble() == Approx(30.0));
    CHECK(values[1].toDouble() == Approx(20.0));
    CHECK(values[2].toDouble() == Approx(20.0));
    CHECK_FALSE(f.model->goToHistory(9));
}

TEST_CASE("the chosen step stays chosen while its values go back and forth",
          "[ui][document-model][history]") {
    Fixture f;
    f.model->selectStep(0);
    f.model->setStepValue(0, "width", "44");
    f.model->waitForCalculation();
    f.model->undo();
    f.model->waitForCalculation();
    CHECK(f.model->currentStep() == 0);
    const QVariantMap width = f.model->stepValues().at(0).toMap();
    CHECK(width["value"].toDouble() == Approx(20.0));
}

TEST_CASE("zooming in gets a finer mesh in the background, zooming out the basic one again",
          "[ui][document-model][detail]") {
    Fixture f;
    const auto basic = f.model->scene()->bodies.at(0);
    CHECK(f.counter->finestChord.load() == Approx(0.01));

    const auto waitFor = [&](int level) {
        QElapsedTimer timer;
        timer.start();
        while (f.model->shownDetailLevel() != level && timer.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        return f.model->shownDetailLevel() == level;
    };

    f.model->setDetailLevel(3);
    REQUIRE(waitFor(3));
    const auto fine = f.model->scene()->bodies.at(0);
    CHECK(fine.id == basic.id);
    CHECK(fine.meshVersion != basic.meshVersion); // the renderer sends the new mesh
    CHECK(f.counter->finestChord.load() == Approx(0.01 / 8.0));

    f.model->setDetailLevel(0);
    REQUIRE(waitFor(0));
    CHECK(f.model->scene()->bodies.at(0).meshVersion == basic.meshVersion);
}

TEST_CASE("a change of the document gives the new body its fine mesh too",
          "[ui][document-model][detail]") {
    Fixture f;
    f.model->setDetailLevel(2);
    QElapsedTimer timer;
    timer.start();
    while (f.model->shownDetailLevel() != 2 && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    REQUIRE(f.model->shownDetailLevel() == 2);

    REQUIRE(f.model->setStepValue(0, "width", "35"));
    f.model->waitForCalculation();
    // the new body is shown at once with the basic mesh, then the fine one follows
    timer.restart();
    const auto version = [&] { return f.model->scene()->bodies.at(0).meshVersion; };
    while (version() % 16 != 2 && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    CHECK(version() % 16 == 2);
}

TEST_CASE("the print bed has the size of the chosen printer", "[ui][document-model][printer]") {
    Fixture f;
    CHECK(f.model->printerId() == "generic");
    CHECK(f.model->scene()->bed.sizeX == Approx(256.0F));

    REQUIRE(f.model->setPrinter("prusa-mk4"));
    CHECK(f.model->printerName() == "Prusa MK4");
    CHECK(f.model->bedX() == Approx(250.0));
    CHECK(f.model->bedY() == Approx(210.0));
    // the scene for the 3D view follows, the bodies stay as they are
    CHECK(f.model->scene()->bed.sizeX == Approx(250.0F));
    CHECK(f.model->scene()->bed.sizeY == Approx(210.0F));
    CHECK(f.model->scene()->bodies.size() == 1);

    // an unknown printer changes nothing
    CHECK_FALSE(f.model->setPrinter("nobody"));
    CHECK(f.model->printerId() == "prusa-mk4");
    // choosing a printer is a setting, not a change of the model: no step in the history
    CHECK_FALSE(f.model->canUndo());
}

TEST_CASE("quick changes never take a body out of the view", "[ui][document-model]") {
    // While a handle is dragged, changes come faster than they are shown: results pile up before
    // the interface applies them. An older result must not sweep away the meshes of a newer one
    // (the body left the view and the selection was let go of).
    Fixture f;
    for (int width = 21; width <= 80; ++width) {
        REQUIRE(f.model->setStepValue(0, "width", QString::number(width)));
        QThread::msleep(6); // the worker finishes some of them while the interface is busy
    }
    f.model->waitForCalculation();

    REQUIRE(f.model->scene() != nullptr);
    CHECK(f.model->scene()->bodies.size() == 1);
    REQUIRE(f.model->elementNames() != nullptr);
    CHECK(f.model->elementNames()->size() == 1);
    CHECK(extentOf(*f.model->scene()).maxX == Approx(40.0));
}

TEST_CASE("combining: parts of a result leave the list, tools of a cut stay as ghosts",
          "[ui][document-model][combine]") {
    Fixture f;
    REQUIRE(f.model->addShape("cylinder", false) == 1);
    f.model->waitForCalculation();
    REQUIRE(f.model->scene()->bodies.size() == 2);

    const QString box = f.model->bodyIdAt(0);
    const QString pin = f.model->bodyIdAt(1);
    const QString cut = f.model->combineBodies({box, pin}, "subtract");
    REQUIRE_FALSE(cut.isEmpty());
    f.model->waitForCalculation();

    // The box is part of the result now; the cylinder stays, as a ghost.
    const QVariantList bodies = f.model->bodies();
    REQUIRE(bodies.size() == 3);
    CHECK(bodies.at(0).toMap()["consumed"].toBool());
    CHECK_FALSE(bodies.at(1).toMap()["consumed"].toBool());
    CHECK_FALSE(bodies.at(2).toMap()["consumed"].toBool());
    CHECK(bodies.at(2).toMap()["cut"].toBool());
    CHECK_FALSE(bodies.at(0).toMap()["cut"].toBool());

    REQUIRE(f.model->scene()->bodies.size() == 2);
    CHECK(f.model->scene()->bodies[0].id == pin.toStdString());
    CHECK(f.model->scene()->bodies[0].color.alpha() < 255);
    CHECK(f.model->scene()->bodies[1].id == cut.toStdString());
    CHECK(f.model->scene()->bodies[1].color.alpha() == 255);

    // The recipe says what happened, and the step is one entry in "last actions".
    CHECK(f.model->recipe().size() == 3);
    CHECK(f.step(2)["type"].toString() == "combine");
    CHECK(f.step(2)["state"].toString() == "ok");
    CHECK(f.step(2)["values"].toList().at(0).toDouble() == Approx(1.0));
    CHECK(f.model->nextUndo()["key"].toString() == "combineBodies");

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->scene()->bodies.size() == 2);
    CHECK_FALSE(f.model->bodies().at(0).toMap()["consumed"].toBool());
    CHECK(f.model->recipe().size() == 2);
}

TEST_CASE("combining: merged bodies all leave the list, bad requests do nothing",
          "[ui][document-model][combine]") {
    Fixture f;
    f.model->addShape("cylinder", false);
    f.model->waitForCalculation();
    const QString box = f.model->bodyIdAt(0);
    const QString pin = f.model->bodyIdAt(1);

    CHECK(f.model->combineBodies({box}, "merge").isEmpty());
    CHECK(f.model->combineBodies({box, pin}, "glue").isEmpty());
    CHECK(f.model->combineBodies({box, "nobody"}, "merge").isEmpty());
    CHECK(f.model->recipe().size() == 2);

    const QString merged = f.model->combineBodies({box, pin}, "merge");
    REQUIRE_FALSE(merged.isEmpty());
    f.model->waitForCalculation();
    REQUIRE(f.model->scene()->bodies.size() == 1);
    CHECK(f.model->scene()->bodies[0].id == merged.toStdString());
    // A body that is part of a result cannot be combined again.
    CHECK(f.model->combineBodies({box, pin}, "merge").isEmpty());
}

TEST_CASE("holes cut live: a hole put down on a body cuts it, in one step",
          "[ui][document-model][combine][acceptance]") {
    Fixture f; // a box 20 x 20 x 20 on the bed
    const int step = f.model->startPlacing("cylinder", true, 0.0, 0.0, 0.0);
    REQUIRE(step >= 0);
    REQUIRE(f.model->setStepValue(step, "radius", "4"));
    REQUIRE(f.model->finishPlacing());
    f.model->waitForCalculation();

    // The recipe has box, cylinder and the cut; there is no step "group".
    REQUIRE(f.model->recipe().size() == 3);
    CHECK(f.step(2)["type"].toString() == "combine");
    CHECK(f.step(2)["state"].toString() == "ok");
    const QVariantList bodies = f.model->bodies();
    CHECK(bodies.at(0).toMap()["consumed"].toBool());
    CHECK(bodies.at(2).toMap()["cut"].toBool());
    // The result is a solid body, the hole stays in the scene as a ghost.
    REQUIRE(f.model->scene()->bodies.size() == 2);
    CHECK(f.model->scene()->bodies[0].color.alpha() < 255);

    // Putting it down was one step to undo.
    CHECK(f.model->historyPosition() == 1);
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 1);
    CHECK(f.model->scene()->bodies.size() == 1);
    REQUIRE(f.model->redo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 3);
}

TEST_CASE("holes cut live: moving the hole away takes the cut back, in one step",
          "[ui][document-model][combine][acceptance]") {
    Fixture f;
    const int step = f.model->startPlacing("cylinder", true, 0.0, 0.0, 0.0);
    f.model->setStepValue(step, "radius", "4");
    f.model->finishPlacing();
    f.model->waitForCalculation();
    REQUIRE(f.model->recipe().size() == 3);
    const int steps = f.model->historyPosition();

    REQUIRE(f.model->setStepValue(step, "x", "100"));
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 2); // the cut is gone ...
    CHECK_FALSE(f.model->bodies().at(0).toMap()["consumed"].toBool());
    CHECK(f.model->historyPosition() == steps + 1); // ... in the very step of the move

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 3);
}

TEST_CASE("holes cut live: switching 'as hole' on and off", "[ui][document-model][combine]") {
    Fixture f;
    REQUIRE(f.model->addShape("cylinder", false) == 1);
    f.model->setStepValue(1, "radius", "4");
    f.model->setStepValue(1, "x", "0");
    f.model->setStepValue(1, "y", "0");
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 2);

    f.model->setBodyHole(1, true);
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 3); // it cuts the box
    CHECK(f.model->nextUndo()["key"].toString() == "holeBody");

    f.model->setBodyHole(1, false);
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 2);
    CHECK(f.model->scene()->bodies.size() == 2);
}

TEST_CASE("a cut body moves as one, its hole with it", "[ui][document-model][combine]") {
    Fixture f;
    const int step = f.model->startPlacing("cylinder", true, 0.0, 0.0, 0.0);
    f.model->setStepValue(step, "radius", "4");
    f.model->finishPlacing();
    f.model->waitForCalculation();
    REQUIRE(f.model->recipe().size() == 3);
    const auto xOf = [&](int index) {
        return f.model->document()
            .features()
            .at(static_cast<std::size_t>(index))
            .params.at("x")
            .expression;
    };

    // By the gizmo: choose the cut body, drag it 30 mm.
    f.model->selectStep(2);
    REQUIRE(f.model->beginMove());
    REQUIRE(f.model->moveCurrent({30.0, std::nullopt, std::nullopt}));
    f.model->commitChange();
    f.model->waitForCalculation();

    CHECK(xOf(0) == "30");
    CHECK(xOf(1) == "30");
    CHECK(f.model->recipe().size() == 3); // the hole went along: it still cuts
    CHECK(f.model->historyPosition() == 2);

    // By align / drop (a list of bodies and distances).
    const QString cut = f.model->bodyIdAt(2);
    REQUIRE(f.model->moveBodiesBy({{cut, {-10.0, 0.0, 0.0}}}, "moveBody"));
    f.model->waitForCalculation();
    CHECK(xOf(0) == "20");
    CHECK(xOf(1) == "20");
    CHECK(f.model->recipe().size() == 3);
}

TEST_CASE("the label of a hole names what it cuts and lets the person change that",
          "[ui][document-model][combine]") {
    Fixture f;
    const int step = f.model->startPlacing("cylinder", true, 0.0, 0.0, 0.0);
    f.model->setStepValue(step, "radius", "4");
    f.model->finishPlacing();
    f.model->waitForCalculation();
    const int hole = 1; // the second body of the document

    QVariantMap label = f.model->cutLabel(hole);
    REQUIRE(label["valid"].toBool());
    CHECK(label["z"].toDouble() == Approx(20.0));
    REQUIRE(label["targets"].toList().size() == 1);
    const QVariantMap target = label["targets"].toList().at(0).toMap();
    CHECK(target["index"].toInt() == 0); // the box it started as
    CHECK(target["cut"].toBool());
    CHECK_FALSE(f.model->cutLabel(0)["valid"].toBool()); // the box is no hole

    // Taking the box off the label: no cut any more, one step, and it stays so.
    const int position = f.model->historyPosition();
    REQUIRE(f.model->setCutTarget(hole, 0, false));
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 2);
    CHECK(f.model->historyPosition() == position + 1);
    CHECK_FALSE(f.model->cutLabel(hole)["targets"].toList().at(0).toMap()["cut"].toBool());
    REQUIRE(f.model->setStepValue(step, "radius", "5")); // a change next to it brings no cut back
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 2);

    REQUIRE(f.model->setCutTarget(hole, 0, true));
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 3);
    CHECK_FALSE(f.model->setCutTarget(0, 0, true)); // the box is no hole
}
