// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Choosing again for a step whose reference needs help (AP-2.10e): the warning with its reason,
// the ghost of the old place, the pick as one undo step, and what is refused.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

#include <memory>

#include "fission/ui/document_model.h"

#include "../naming/judge.h"

using namespace fission::test;
using fission::ui::DocumentModel;
namespace core = fission::core;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// A box with a slot that is first too shallow to reach the top, the cut, and a step that points
// at the top face of the cut body. Growing the slot splits the top in two.
struct Repair {
    Repair() {
        application();
        kernel = fission::geo::occt::makeKernel();
        std::vector<std::unique_ptr<const core::FeatureType>> extra;
        extra.push_back(std::make_unique<MarkFaceType>());
        model = std::make_unique<DocumentModel>(kernel, std::move(extra));

        plate = world.plate("40", "30", "10");
        slot = world.add(
            "box", {{"width", "10"}, {"depth", "60"}, {"height", "4"}, {"x", "0"}, {"z", "5"}});
        cut = world.add("combine", {}, {world.bodyOf(plate), world.bodyOf(slot)},
                        {{"mode", "subtract"}});
        const core::RecomputeResult first = world.run();
        mark = world.mark_(cut, world.pick(first, cut, core::ElementKind::face, "F1/box/top"));
        model->replaceDocument(world.document);
        model->waitForCalculation();
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }
    QString state(int index) const { return step(index).value("state").toString(); }

    // The slot grows through the top: the top is two pieces and the reference needs help.
    void breakIt() {
        REQUIRE(model->setStepValue(1, QStringLiteral("height"), QStringLiteral("20")));
        model->waitForCalculation();
    }

    // The number of one of the two top pieces, as the pick of a click would report it.
    int aTopPiece() {
        REQUIRE(world.document.setFeatureValue(slot, "height", "20"));
        const core::RecomputeResult result = world.run();
        const Truth pieces = facesLookingAt(world.looksOf(result, cut), {0, 0, 1}, 10.0);
        REQUIRE(pieces.size() == 2);
        return static_cast<int>(pieces.front());
    }

    RefWorld world;
    core::FeatureId plate, slot, cut, mark;
    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
};

} // namespace

TEST_CASE("repair: a split face shows the reason and can be chosen again", "[ui][repair]") {
    Repair f;
    REQUIRE(f.state(3) == "ok");
    CHECK_FALSE(f.model->repairing());
    f.breakIt();

    REQUIRE(f.state(3) == "warning");
    const QVariantMap problem = f.step(3).value("problem").toMap();
    CHECK(problem.value("code").toString() == "referenceNeedsHelp");
    CHECK(problem.value("reason").toString() == "split");
    CHECK(problem.value("kind").toString() == "face");

    // Steps that have nothing to repair cannot be repaired.
    CHECK_FALSE(f.model->startRepair(0));
    CHECK_FALSE(f.model->startRepair(99));

    REQUIRE(f.model->startRepair(3));
    CHECK(f.model->repairing());
    CHECK(f.model->repairKind() == "face");
    const QVariantMap ghost = f.model->repairGhost();
    CHECK(ghost.value("valid").toBool());
    CHECK(ghost.value("kind").toString() == "face");
    CHECK(ghost.value("points").toList().size() == 4);

    const QString body = f.model->bodyOfStep(2);
    const int piece = f.aTopPiece();
    // Wrong kind and wrong body are refused and the repair goes on.
    CHECK_FALSE(f.model->completeRepair(body, "edge", piece));
    CHECK_FALSE(f.model->completeRepair(f.model->bodyOfStep(0), "face", piece));
    CHECK_FALSE(f.model->completeRepair(body, "face", 0));
    CHECK(f.model->repairing());

    const int stepsBefore = f.model->historyPosition();
    REQUIRE(f.model->completeRepair(body, "face", piece));
    f.model->waitForCalculation();
    CHECK_FALSE(f.model->repairing());
    CHECK(f.state(3) == "ok");
    CHECK(f.model->historyPosition() == stepsBefore + 1); // one undo step

    // Undo brings the warning back.
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.state(3) == "warning");
}

TEST_CASE("repair: cancelling and a problem that goes away end the repair", "[ui][repair]") {
    Repair f;
    f.breakIt();
    REQUIRE(f.model->startRepair(3));
    f.model->cancelRepair();
    CHECK_FALSE(f.model->repairing());
    CHECK(f.state(3) == "warning"); // nothing changed

    // The slot gets shallow again by itself: nothing is left to choose.
    REQUIRE(f.model->startRepair(3));
    REQUIRE(f.model->setStepValue(1, QStringLiteral("height"), QStringLiteral("4")));
    f.model->waitForCalculation();
    CHECK_FALSE(f.model->repairing());
    CHECK(f.state(3) == "ok");
}
