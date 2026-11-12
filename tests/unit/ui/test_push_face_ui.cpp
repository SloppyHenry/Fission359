// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pulling a face from the interface (AP-2.4a): one step and one undo, a grip on the face, bad
// requests change nothing.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

#include <cmath>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"

using Catch::Approx;
using fission::ui::DocumentModel;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct Pull {
    Pull() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 × 20 × 20 mm: step 0
        model->waitForCalculation();
        body = model->bodyOfStep(0);
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    QString body;
};

} // namespace

TEST_CASE("push face: one pull is one step and one undo", "[ui][faces]") {
    Pull f;
    const int before = f.model->historyPosition();
    const QString made = f.model->pushFace(f.body, 1);
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.model->historyPosition() == before + 1);
    CHECK(f.step(1).value("type").toString() == "pushFace");
    CHECK(f.step(1).value("state").toString() == "ok");

    // The body is the result now; the box itself is part of it.
    CHECK(f.model->pushFace(f.body, 1).isEmpty());

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("push face: a bad request is refused and changes nothing", "[ui][faces]") {
    Pull f;
    const int before = f.model->historyPosition();
    CHECK(f.model->pushFace(f.body, 0).isEmpty());
    CHECK(f.model->pushFace(f.body, 4242).isEmpty());
    CHECK(f.model->pushFace(QStringLiteral("nobody"), 1).isEmpty());
    CHECK(f.model->historyPosition() == before);
}

TEST_CASE("push face: a body with rounded corners is stretched, its corners stay", "[ui][faces]") {
    Pull f;
    REQUIRE(f.model->roundVerticalEdges(f.body) == f.body); // the box gets a corner radius
    f.model->waitForCalculation();

    // Some flat face of it: the first one that can be pulled makes a stretching step.
    QString made;
    for (int number = 1; number <= 12 && made.isEmpty(); ++number) {
        made = f.model->pushFace(f.body, number);
    }
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.step(1).value("type").toString() == "stretch");
    CHECK(f.step(1).value("state").toString() == "ok");
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("offset faces: no face given moves all faces of the body", "[ui][faces]") {
    Pull f;
    const QString made = f.model->offsetFaces(f.body, {});
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.step(1).value("type").toString() == "offsetFaces");
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->offsetFaces(QStringLiteral("nobody"), {}).isEmpty());
}

TEST_CASE("tilt face: a flat face turns about its lowest edge", "[ui][faces]") {
    Pull f;
    QString made;
    for (int number = 1; number <= 6 && made.isEmpty(); ++number) {
        made = f.model->tiltFace(f.body, number);
    }
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.step(1).value("type").toString() == "tiltFace");
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->tiltFace(f.body, 0).isEmpty());
    CHECK(f.model->tiltFace(QStringLiteral("nobody"), 1).isEmpty());
}

TEST_CASE("drill hole: a flat face gets a hole step", "[ui][faces][holes]") {
    Pull f;
    QString made;
    for (int number = 1; number <= 6 && made.isEmpty(); ++number) {
        made = f.model->drillHole(f.body, number);
    }
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.step(1).value("type").toString() == "hole");
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->drillHole(f.body, 0).isEmpty());
    CHECK(f.model->drillHole(QStringLiteral("nobody"), 1).isEmpty());
}

TEST_CASE("drill hole: the screw size and kind come from the tables", "[ui][faces][holes]") {
    Pull f;
    QString made;
    for (int number = 1; number <= 6 && made.isEmpty(); ++number) {
        made = f.model->drillHole(f.body, number);
    }
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    const int index = static_cast<int>(f.model->recipe().size()) - 1;
    CHECK(f.model->setHoleScrew(index, QStringLiteral("M3"), QStringLiteral("countersink")));
    f.model->waitForCalculation();
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->setHoleScrew(index, QStringLiteral("M4"), QStringLiteral("counterbore")));
    f.model->waitForCalculation();
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->setHoleScrew(index, QStringLiteral("M3"), QStringLiteral("nutTrap")));
    f.model->waitForCalculation();
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->setHoleScrew(index, QStringLiteral("M3"), QStringLiteral("tapDrill")));
    f.model->waitForCalculation();
    CHECK(f.step(1).value("state").toString() == "ok");
    // One undo step goes back to the previous kind.
    CHECK(f.model->undo());
    CHECK_FALSE(f.model->setHoleScrew(index, QStringLiteral("M99"), QStringLiteral("plain")));
    CHECK_FALSE(f.model->setHoleScrew(index, QStringLiteral("M3"), QStringLiteral("round")));
    CHECK_FALSE(f.model->setHoleScrew(0, QStringLiteral("M3"), QStringLiteral("plain")));
}

TEST_CASE("remove faces: a bad request is refused, a flat face is its own feature", "[ui][faces]") {
    Pull f;
    const int before = f.model->historyPosition();
    CHECK(f.model->removeFaces(f.body, {}).isEmpty());
    CHECK(f.model->removeFaces(f.body, QVariantList{4242}).isEmpty());
    CHECK(f.model->removeFaces(QStringLiteral("nobody"), QVariantList{1}).isEmpty());
    CHECK(f.model->historyPosition() == before);

    const QVariantList feature = f.model->faceFeature(f.body, 1);
    REQUIRE(feature.size() == 1);
    CHECK(feature.first().toInt() == 1);
    CHECK(f.model->faceFeature(f.body, 999).isEmpty());
}

TEST_CASE("shell: a body is hollowed with the top open, one step and one undo", "[ui][hollow]") {
    Pull f;
    const int before = f.model->historyPosition();
    const QString made = f.model->shellBody(f.body, {});
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.model->historyPosition() == before + 1);
    CHECK(f.step(1).value("type").toString() == "shell");
    CHECK(f.step(1).value("state").toString() == "ok");
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 1);
    CHECK(f.model->shellBody(QStringLiteral("nobody"), {}).isEmpty());
}

TEST_CASE("draft: no faces given drafts the sides of the body", "[ui][draft]") {
    Pull f;
    const QString made = f.model->draftFaces(f.body, {});
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.step(1).value("type").toString() == "draft");
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.model->draftFaces(QStringLiteral("nobody"), {}).isEmpty());
}
