// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Putting shapes down (AP-1.5c): the shape follows the pointer, lands on the bed or on the flat
// top of a body, is one step in the history, and can be a hole.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

#include <algorithm>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/placement_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::PlacementController;
using fission::ui::Viewport3D;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct Box3 {
    double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300, minZ = 1e300, maxZ = -1e300;
};

// The extent of the body with this id in the scene.
Box3 extentOf(const fission::render::RenderScene& scene, const std::string& id) {
    Box3 box;
    for (const auto& body : scene.bodies) {
        if (body.id != id) {
            continue;
        }
        for (const auto& p : body.geometry().positions) {
            box.minX = std::min(box.minX, static_cast<double>(p.x));
            box.maxX = std::max(box.maxX, static_cast<double>(p.x));
            box.minY = std::min(box.minY, static_cast<double>(p.y));
            box.maxY = std::max(box.maxY, static_cast<double>(p.y));
            box.minZ = std::min(box.minZ, static_cast<double>(p.z));
            box.maxZ = std::max(box.maxZ, static_cast<double>(p.z));
        }
    }
    return box;
}

struct Fixture {
    Fixture() {
        application();
        std::shared_ptr<const fission::geo::GeometryKernel> kernel =
            fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox();
        model->waitForCalculation();
        viewport.setProperty("animated", false);
        viewport.setWidth(1000.0);
        viewport.setHeight(600.0);
        viewport.setCamera(0.0, 90.0, 400.0); // straight down onto the bed
        viewport.setScene(model->scene());
        placement = std::make_unique<PlacementController>(model.get());
        placement->attach(&viewport);
    }

    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
    }

    // Window pixel of a point of the world.
    QPointF pixel(double x, double y, double z) const { return viewport.project(x, y, z); }

    int stepsDone() const {
        int done = 0;
        for (const QVariant& step : model->history()) {
            done += step.toMap()["done"].toBool() ? 1 : 0;
        }
        return done;
    }

    std::string lastBodyId() const {
        return model->bodyIdAt(static_cast<int>(model->bodies().size()) - 1).toStdString();
    }

    std::unique_ptr<DocumentModel> model;
    Viewport3D viewport;
    std::unique_ptr<PlacementController> placement;
};

} // namespace

TEST_CASE("placing: a shape put down is one step, undo takes it away", "[ui][placement]") {
    Fixture f;
    REQUIRE(f.model->startPlacing("sphere", false, 40.0, 25.0, 0.0) >= 0);
    CHECK(f.model->placing());
    f.model->placeAt(50.0, 30.0, 0.0, {});
    f.model->placeAt(60.0, 35.0, 0.0, {});
    CHECK(f.stepsDone() == 0); // still being put down: nothing in the history yet
    REQUIRE(f.model->finishPlacing());
    f.refresh();

    CHECK_FALSE(f.model->placing());
    CHECK(f.model->recipe().size() == 2);
    CHECK(f.model->bodies().size() == 2);
    CHECK(f.stepsDone() == 1); // one step for the whole drag
    const Box3 sphere = extentOf(*f.model->scene(), f.lastBodyId());
    CHECK((sphere.minX + sphere.maxX) / 2.0 == Approx(60.0).margin(0.05)); // a mesh, not exact
    CHECK((sphere.minY + sphere.maxY) / 2.0 == Approx(35.0).margin(0.05));
    CHECK(sphere.minZ == Approx(0.0).margin(1e-3)); // it rests on the bed
    CHECK(f.model->nextUndo()["key"].toString() == "addFeature");
    CHECK(f.model->nextUndo()["type"].toString() == "sphere");

    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.model->recipe().size() == 1);
    CHECK(f.model->bodies().size() == 1);
    REQUIRE(f.model->redo());
    f.refresh();
    CHECK(f.model->recipe().size() == 2);
}

TEST_CASE("placing: Esc leaves nothing behind", "[ui][placement]") {
    Fixture f;
    REQUIRE(f.model->startPlacing("cone", false, 50.0, 0.0, 0.0) >= 0);
    f.model->placeAt(70.0, 10.0, 0.0, {});
    f.refresh();
    CHECK(f.model->bodies().size() == 2); // it is there while it is dragged ...
    f.model->cancelPlacing();
    f.refresh();

    CHECK_FALSE(f.model->placing());
    CHECK(f.model->recipe().size() == 1); // ... and gone afterwards
    CHECK(f.model->bodies().size() == 1);
    CHECK(f.stepsDone() == 0);
    CHECK_FALSE(f.model->canUndo());
}

TEST_CASE("placing: an unknown shape is refused", "[ui][placement]") {
    Fixture f;
    CHECK(f.model->startPlacing("teapot", false, 0, 0, 0) == -1);
    CHECK_FALSE(f.model->placing());
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("placing: a click puts the shape on a free place of the bed", "[ui][placement]") {
    Fixture f;
    const int step = f.model->addShape("cylinder", false);
    f.refresh();
    REQUIRE(step == 1);
    CHECK(f.stepsDone() == 1);
    // The cube of the start stands in the middle: the new one does not stand on it.
    const Box3 cube = extentOf(*f.model->scene(), f.model->bodyIdAt(0).toStdString());
    const Box3 cylinder = extentOf(*f.model->scene(), f.lastBodyId());
    const bool apart = cylinder.minX >= cube.maxX || cylinder.maxX <= cube.minX ||
                       cylinder.minY >= cube.maxY || cylinder.maxY <= cube.minY;
    CHECK(apart);
    CHECK(cylinder.minZ == Approx(0.0).margin(1e-3));

    // The next one finds the next free place.
    f.model->addShape("cylinder", false);
    f.refresh();
    const Box3 second = extentOf(*f.model->scene(), f.lastBodyId());
    CHECK((second.minX != cylinder.minX || second.minY != cylinder.minY));
}

TEST_CASE("holes: a shape can be put down as a hole and the mark can change", "[ui][placement]") {
    Fixture f;
    REQUIRE(f.model->startPlacing("cylinder", true, 40.0, 0.0, 0.0) >= 0);
    REQUIRE(f.model->finishPlacing());
    f.refresh();

    CHECK(f.model->bodies().at(1).toMap()["hole"].toBool());
    CHECK_FALSE(f.model->bodies().at(0).toMap()["hole"].toBool());
    CHECK(f.stepsDone() == 1); // "added as a hole" is not two steps
    // A hole is drawn see-through, a part is not.
    CHECK(f.model->scene()->bodies.at(1).color.alpha() < 255);
    CHECK(f.model->scene()->bodies.at(0).color.alpha() == 255);

    f.model->setBodyHole(1, false);
    f.refresh();
    CHECK_FALSE(f.model->bodies().at(1).toMap()["hole"].toBool());
    CHECK(f.model->scene()->bodies.at(1).color.alpha() == 255);
    CHECK(f.model->nextUndo()["key"].toString() == "solidBody");
    f.model->undo();
    f.refresh();
    CHECK(f.model->bodies().at(1).toMap()["hole"].toBool());
    CHECK(f.model->nextUndo()["key"].toString() == "addFeature");
}

TEST_CASE("landing: bed, flat top of a body, or the work plane", "[ui][placement]") {
    Fixture f;
    // Straight above the cube of the start (20 mm, standing on the bed in the middle).
    const auto onTop = f.viewport.landingAt(f.pixel(0, 0, 20).x(), f.pixel(0, 0, 20).y(), {}, 0.0);
    REQUIRE(onTop.has_value());
    CHECK(onTop->z == Approx(20.0).margin(1e-3));
    CHECK_FALSE(onTop->surface.isEmpty());

    // Beside it: the bed.
    const auto onBed =
        f.viewport.landingAt(f.pixel(60, 40, 0).x(), f.pixel(60, 40, 0).y(), {}, 0.0);
    REQUIRE(onBed.has_value());
    CHECK(onBed->z == Approx(0.0).margin(1e-6));
    CHECK(onBed->x == Approx(60.0).margin(0.05));
    CHECK(onBed->y == Approx(40.0).margin(0.05));
    CHECK(onBed->surface.isEmpty());

    // The body being put down is looked through: the bed lies below it.
    const QString cube = f.model->bodyIdAt(0);
    const auto through =
        f.viewport.landingAt(f.pixel(0, 0, 20).x(), f.pixel(0, 0, 20).y(), cube, 0.0);
    REQUIRE(through.has_value());
    CHECK(through->z == Approx(0.0).margin(1e-6));

    // Far outside the bed: the work plane.
    const auto outside = f.viewport.landingAt(2.0, 2.0, {}, 12.0);
    if (outside) {
        CHECK(outside->z == Approx(12.0).margin(1e-6));
    }
}

TEST_CASE("placing: the controller follows the pointer and the surface becomes the work plane",
          "[ui][placement]") {
    Fixture f;
    const QPointF bed = f.pixel(60, 40, 0);
    REQUIRE(f.placement->begin("box", false, bed.x(), bed.y()));
    CHECK(f.placement->active());
    f.refresh();

    // Over the bed it stands on the bed, at whole millimetres.
    {
        const Box3 box = extentOf(*f.model->scene(), f.lastBodyId());
        CHECK(box.minZ == Approx(0.0).margin(1e-3));
        CHECK(std::abs((box.minX + box.maxX) / 2.0 - 60.0) <= 1.0);
    }

    // Dragged over the cube: it stands on the cube's top.
    const QPointF top = f.pixel(2, 2, 20);
    f.placement->moveTo(top.x(), top.y());
    f.refresh();
    {
        const Box3 box = extentOf(*f.model->scene(), f.lastBodyId());
        CHECK(box.minZ == Approx(20.0).margin(1e-3));
    }
    REQUIRE(f.placement->finish());
    CHECK(f.model->workplaneZ() == Approx(20.0).margin(1e-3));
    CHECK_FALSE(f.model->workplaneBodyId().isEmpty());
    CHECK(f.stepsDone() == 1);

    // Back on the bed: the work plane is the bed again.
    REQUIRE(f.placement->begin("sphere", false, bed.x(), bed.y()));
    REQUIRE(f.placement->finish());
    CHECK(f.model->workplaneZ() == Approx(0.0).margin(1e-6));
    CHECK(f.model->workplaneBodyId().isEmpty());
}

TEST_CASE("placing: lettering has words and a font to change", "[ui][placement]") {
    Fixture f;
    const int step = f.model->addShape("text", false);
    f.refresh();
    REQUIRE(step >= 0);
    f.model->selectStep(step);
    const QVariantList texts = f.model->stepTexts();
    REQUIRE(texts.size() == 2);
    CHECK(texts[0].toMap()["name"].toString() == "text");
    CHECK(texts[0].toMap()["value"].toString() == "Text");
    CHECK(texts[1].toMap()["name"].toString() == "font");

    REQUIRE(f.model->setStepText(step, "text", "LENA"));
    f.refresh();
    CHECK(f.model->stepTexts()[0].toMap()["value"].toString() == "LENA");
    CHECK(f.model->nextUndo()["key"].toString() == "setText");
    f.model->undo();
    f.refresh();
    CHECK(f.model->stepTexts()[0].toMap()["value"].toString() == "Text");

    // A name the step does not have is refused.
    CHECK_FALSE(f.model->setStepText(step, "colour", "red"));
}

TEST_CASE("placing: a count of sides has no unit, lengths do", "[ui][placement]") {
    Fixture f;
    const int step = f.model->addShape("prism", false);
    f.refresh();
    f.model->selectStep(step);
    const QVariantList values = f.model->stepValues();
    REQUIRE(values.size() >= 3);
    CHECK(values[0].toMap()["name"].toString() == "sides");
    CHECK(values[0].toMap()["kind"].toString() == "plain");
    CHECK(values[1].toMap()["kind"].toString() == "length");
}
