// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
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
        for (const auto& point : body.mesh.positions) {
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

    fission::geo::NamedSolid makeBox(double x, double y, double z,
                                     fission::geo::Point3d corner) const override {
        ++builds;
        return inner_->makeBox(x, y, z, corner);
    }
    fission::geo::NamedSolid makeCylinder(double radius, double height,
                                          fission::geo::Point3d base) const override {
        ++builds;
        return inner_->makeCylinder(radius, height, base);
    }
    fission::geo::SolidReport measure(const fission::geo::Solid& solid) const override {
        return inner_->measure(solid);
    }
    fission::geo::TriangleMesh
    tessellate(const fission::geo::Solid& solid,
               const fission::geo::TessellationParams& p) const override {
        return inner_->tessellate(solid, p);
    }

    mutable std::atomic<int> builds{0};

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
    REQUIRE(values.size() == 6); // width depth height x y z
    CHECK(values[0].toMap()["name"].toString() == "width");
    CHECK(values[0].toMap()["group"].toString() == "size");
    CHECK(values[0].toMap()["expression"].toString().isEmpty()); // typed as a plain number
    CHECK(values[3].toMap()["group"].toString() == "position");

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
