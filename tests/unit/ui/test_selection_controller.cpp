// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The selection in the interface (AP-1.4): what a click chooses, what stays chosen when the
// document is recalculated, and how the choice and the recipe follow each other. The 3D view is
// not drawn here; its answers are sent by hand.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/viewport_3d.h"

using fission::core::SelectableKind;
using fission::ui::DocumentModel;
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
    DocumentModel model{
        std::shared_ptr<const fission::geo::GeometryKernel>(fission::geo::occt::makeKernel())};
    SelectionController selection{&model};
    Viewport3D viewport;

    World() {
        application();
        model.addDemoBox();
        model.waitForCalculation();
        selection.attach(&viewport);
        viewport.setScene(model.scene());
    }

    QString body() const { return model.bodyIdAt(0); }

    // The 3D view found something under the pointer.
    void click(SelectableKind kind, int number, bool extend = false) {
        emit viewport.picked(static_cast<int>(Viewport3D::PickIntent::click), extend, body(),
                             static_cast<int>(kind), number);
    }
    void doubleClick(SelectableKind kind, int number) {
        emit viewport.picked(static_cast<int>(Viewport3D::PickIntent::doubleClick), false, body(),
                             static_cast<int>(kind), number);
    }
    void clickBackground() {
        emit viewport.picked(static_cast<int>(Viewport3D::PickIntent::click), false, QString(), 0,
                             0);
    }

    // The number of the face with this name in the current mesh.
    int faceNumber(const std::string& name) const {
        const auto names = model.elementNames();
        return static_cast<int>(
            names->at(fission::core::BodyId{body().toStdString()}).findFace(name).value());
    }

    QVariantMap first() const { return selection.items().front().toMap(); }
};

} // namespace

TEST_CASE("a click on a face chooses it and the recipe shows its step", "[selection][ui]") {
    World world;
    REQUIRE(world.selection.count() == 0);
    REQUIRE(world.model.currentStep() == -1);

    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    REQUIRE(world.selection.count() == 1);
    const QVariantMap item = world.first();
    CHECK(item["kind"].toString() == "face");
    CHECK(item["name"].toString() == "F1/box/top");
    CHECK(item["bodyIndex"].toInt() == 0);
    CHECK(world.selection.singleFaceChosen());
    CHECK(world.model.currentStep() == 0); // the panel on the right shows its values

    world.clickBackground();
    CHECK(world.selection.count() == 0);
    CHECK(world.model.currentStep() == -1);
}

TEST_CASE("a double click chooses the whole body", "[selection][ui]") {
    World world;
    world.doubleClick(SelectableKind::face, world.faceNumber("F1/box/top"));
    REQUIRE(world.selection.count() == 1);
    CHECK(world.first()["kind"].toString() == "body");
    CHECK_FALSE(world.selection.singleFaceChosen());
}

TEST_CASE("a chosen face stays chosen when a value changes", "[selection][ui]") {
    World world;
    const int top = world.faceNumber("F1/box/top");
    world.click(SelectableKind::face, top);
    REQUIRE(world.model.setStepValue(0, "width", "35"));
    world.model.waitForCalculation();

    REQUIRE(world.selection.count() == 1);
    CHECK(world.first()["name"].toString() == "F1/box/top");
    // It is the same face of the new body, wherever its number ended up.
    CHECK(world.first()["number"].toInt() == world.faceNumber("F1/box/top"));
    CHECK(world.model.scene()->bodies.size() == 1);
}

TEST_CASE("a chosen edge and corner stay chosen when a value changes", "[selection][ui]") {
    World world;
    const auto names = world.model.elementNames();
    const auto& named = names->at(fission::core::BodyId{world.body().toStdString()});
    const std::uint32_t edge = 4;
    const std::uint32_t corner = 3;
    REQUIRE_FALSE(named.edgeName(edge).empty());
    REQUIRE_FALSE(named.cornerName(corner).empty());
    const std::string edgeName = named.edgeName(edge);
    const std::string cornerName = named.cornerName(corner);

    world.click(SelectableKind::edge, static_cast<int>(edge));
    world.click(SelectableKind::vertex, static_cast<int>(corner), true);
    REQUIRE(world.selection.count() == 2);
    REQUIRE(world.model.setStepValue(0, "height", "3 cm"));
    world.model.waitForCalculation();

    REQUIRE(world.selection.count() == 2);
    const auto items = world.selection.items();
    CHECK(items[0].toMap()["name"].toString().toStdString() == edgeName);
    CHECK(items[1].toMap()["name"].toString().toStdString() == cornerName);
}

TEST_CASE("a hidden body cannot stay chosen", "[selection][ui]") {
    World world;
    world.doubleClick(SelectableKind::face, world.faceNumber("F1/box/top"));
    REQUIRE(world.selection.count() == 1);
    world.model.setBodyVisible(0, false);
    CHECK(world.selection.count() == 0);
}

TEST_CASE("choosing a step in the recipe chooses its body, and the other way round",
          "[selection][ui]") {
    World world;
    world.model.selectStep(0);
    REQUIRE(world.selection.count() == 1);
    CHECK(world.first()["kind"].toString() == "body");

    world.model.selectStep(-1);
    CHECK(world.selection.count() == 0);

    world.selection.chooseBody(0, false);
    CHECK(world.selection.count() == 1);
    CHECK(world.model.currentStep() == 0);

    // ⇧ on the body in the list takes it away again.
    world.selection.chooseBody(0, true);
    CHECK(world.selection.count() == 0);
    CHECK(world.model.currentStep() == -1);
}

TEST_CASE("the filter lets go of what it does not allow and tells the view", "[selection][ui]") {
    World world;
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    REQUIRE(world.selection.count() == 1);
    CHECK(world.selection.filter() == "all");

    world.selection.setFilter(QStringLiteral("edge"));
    CHECK(world.selection.filter() == "edge");
    CHECK(world.selection.count() == 0);
    CHECK(world.viewport.selectionFilter() ==
          fission::core::SelectionFilter::only(SelectableKind::edge));

    world.selection.setFilter(QStringLiteral("nonsense")); // anything unknown means "all"
    CHECK(world.selection.filter() == "all");
}

TEST_CASE("select all chooses every shown body, Esc and clear choose nothing", "[selection][ui]") {
    World world;
    world.selection.chooseAllBodies();
    CHECK(world.selection.count() == 1);
    world.selection.clear();
    CHECK(world.selection.count() == 0);
    CHECK(world.model.currentStep() == -1);
}

TEST_CASE("the view lights up what is chosen", "[selection][ui]") {
    World world;
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    const auto snapshot = world.viewport.takeSnapshot(1.0F);
    REQUIRE(snapshot.highlights.size() == 1);
    CHECK(snapshot.highlights[0].kind == SelectableKind::face);
    CHECK_FALSE(snapshot.highlights[0].hover);
    CHECK(snapshot.highlights[0].body == world.body().toStdString());
}

TEST_CASE("looking at the chosen face turns the view squarely to it", "[selection][ui]") {
    World world;
    world.viewport.setProperty("animated", false);
    CHECK_FALSE(world.selection.lookAtChosenFace()); // nothing chosen
    world.click(SelectableKind::face, world.faceNumber("F1/box/top"));
    CHECK(world.selection.lookAtChosenFace());
    CHECK(world.viewport.cameraPitch() > 89.0); // looking down at the top
}
