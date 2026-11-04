// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Rounding and bevelling from the interface (AP-2.5): the edges that are taken along, one undo
// step per rounding, a fresh box keeps being a box, and a size that is too large shows its
// number and place so that the message can offer the largest value (AP-2.14).

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

struct Blend {
    Blend() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 × 20 × 20 mm: step 0
        model->waitForCalculation();
        body = model->bodyOfStep(0);
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }
    double value(int index, const char* name) {
        model->selectStep(index);
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap()["name"].toString() == QLatin1String(name)) {
                return entry.toMap()["value"].toDouble();
            }
        }
        return -1.0;
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    QString body;
};

} // namespace

TEST_CASE("blend edges: vertical edges and the edges of a face are found", "[ui][blend]") {
    Blend f;
    CHECK(f.model->edgesOf(f.body, QStringLiteral("vertical"), 0).size() == 4);
    CHECK(f.model->edgesOf(f.body, QStringLiteral("face"), 1).size() == 4);
    CHECK(f.model->edgesOf(f.body, QStringLiteral("face"), 999).isEmpty());
    CHECK(f.model->edgesOf(QStringLiteral("nobody"), QStringLiteral("vertical"), 0).isEmpty());
    const QVariantList vertical = f.model->edgesOf(f.body, QStringLiteral("vertical"), 0);
    CHECK(f.model->edgesOf(f.body, QStringLiteral("chain"), vertical.first().toInt()).size() == 1);
}

TEST_CASE("blend edges: one rounding is one step and one undo", "[ui][blend]") {
    Blend f;
    const int before = f.model->historyPosition();
    const QVariantList edges = f.model->edgesOf(f.body, QStringLiteral("vertical"), 0);
    const QString made = f.model->blendEdges(f.body, edges, QStringLiteral("fillet"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.model->historyPosition() == before + 1);
    CHECK(f.step(1).value("type").toString() == "fillet");
    CHECK(f.step(1).value("state").toString() == "ok");
    CHECK(f.value(1, "radius") == Approx(2.0)); // 0.3 of 20 mm is more than 2: the upper limit

    // The result is the body now; the box itself is part of it.
    CHECK(f.model->blendEdges(f.body, edges, QStringLiteral("fillet")).isEmpty());

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("blend edges: a bad request is refused and changes nothing", "[ui][blend]") {
    Blend f;
    const int before = f.model->historyPosition();
    const QVariantList edges = f.model->edgesOf(f.body, QStringLiteral("vertical"), 0);
    CHECK(f.model->blendEdges(f.body, {}, QStringLiteral("fillet")).isEmpty());
    CHECK(f.model->blendEdges(f.body, edges, QStringLiteral("melt")).isEmpty());
    CHECK(f.model->blendEdges(QStringLiteral("nobody"), edges, QStringLiteral("fillet")).isEmpty());
    CHECK(f.model->blendEdges(f.body, QVariantList{4242}, QStringLiteral("chamfer")).isEmpty());
    CHECK(f.model->historyPosition() == before);
}

TEST_CASE("blend edges: a fresh box keeps being a box when its vertical edges are rounded",
          "[ui][blend]") {
    Blend f;
    const int before = f.model->historyPosition();
    CHECK(f.model->roundVerticalEdges(f.body) == f.body);
    f.model->waitForCalculation();
    CHECK(f.model->historyPosition() == before + 1);
    CHECK(f.model->recipe().size() == 1); // no new step: the box has a corner radius now
    CHECK(f.value(0, "cornerRadius") == Approx(4.0)); // a fifth of 20 mm

    // With rounded corners it is no fresh box any more: the edges get a rounding step.
    const QString again = f.model->roundVerticalEdges(f.body);
    f.model->waitForCalculation();
    CHECK_FALSE(again.isEmpty());
    CHECK(f.step(1).value("type").toString() == "fillet");

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.value(0, "cornerRadius") == Approx(4.0));
}

TEST_CASE("blend edges: a size that is too large shows the number, the place and the edge",
          "[ui][blend]") {
    Blend f;
    const QVariantList edges = f.model->edgesOf(f.body, QStringLiteral("vertical"), 0);
    REQUIRE_FALSE(f.model->blendEdges(f.body, edges, QStringLiteral("fillet")).isEmpty());
    f.model->waitForCalculation();
    REQUIRE(f.model->setStepValue(1, QStringLiteral("radius"), QStringLiteral("60")));
    f.model->waitForCalculation();

    const QVariantMap step = f.step(1);
    CHECK(step.value("state").toString() == "fails");
    const QVariantMap problem = step.value("problem").toMap();
    CHECK(problem.value("field").toString() == "radius");
    CHECK(problem.value("numbers").toMap().value("given").toDouble() == Approx(60.0));
    CHECK(problem.value("numbers").toMap().value("maxValue").toDouble() > 0.0);
    CHECK(problem.value("where").toMap().contains(QStringLiteral("x")));
    CHECK(problem.value("elements").toStringList().size() == 4);

    // "Use the largest value" works and is one undo step.
    const int before = f.model->historyPosition();
    REQUIRE(f.model->useMaximum(1));
    f.model->waitForCalculation();
    CHECK(f.model->historyPosition() == before + 1);
    CHECK(f.step(1).value("state").toString() == "ok");
}

TEST_CASE("blend edges: the step has a grip at its edge that changes the size", "[ui][blend]") {
    Blend f;
    const QVariantList edges = f.model->edgesOf(f.body, QStringLiteral("vertical"), 0);
    REQUIRE_FALSE(f.model->blendEdges(f.body, edges, QStringLiteral("chamfer")).isEmpty());
    f.model->waitForCalculation();
    f.model->selectStep(1);

    const QVariantList handles = f.model->stepHandles();
    REQUIRE(handles.size() == 1);
    const QVariantMap grip = handles.first().toMap();
    CHECK(grip.value("parameter").toString() == "distance");
    CHECK(grip.value("role").toString() == "rounding");
    CHECK(grip.value("value").toDouble() == Approx(2.0));
    // It points into the body: the edge is at a corner of the cube, so every part of the way
    // is towards the middle (x and y both grow or both shrink, z stays).
    CHECK(std::abs(grip.value("dz").toDouble()) < 1e-6);
    CHECK(std::hypot(grip.value("dx").toDouble(), grip.value("dy").toDouble()) == Approx(1.0));

    // The anchor values are not shown in the panel's values.
    for (const QVariant& entry : f.model->stepValues()) {
        const QVariantMap value = entry.toMap();
        if (value.value("name").toString().startsWith(QLatin1String("anchor"))) {
            CHECK(value.value("group").toString() == "hidden");
        }
    }
}
