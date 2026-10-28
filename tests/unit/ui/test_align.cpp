// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The points that align several bodies (AP-1.6e): shown when two or more bodies are chosen, a
// click puts them flush or in the middle, one step in the history.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/align_controller.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::AlignController;
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

// Three boxes: the demo box in the middle, one at x = 60 (30 wide), one at x = -50.
struct Fixture {
    Fixture() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox();
        model->waitForCalculation();
        for (const char* x : {"60", "-50"}) {
            const int step = model->addShape(QStringLiteral("box"), false);
            REQUIRE(step >= 0);
            REQUIRE(model->setStepValue(step, QStringLiteral("x"), QLatin1String(x)));
            REQUIRE(model->setStepValue(step, QStringLiteral("y"), QStringLiteral("0")));
            REQUIRE(model->setStepValue(step, QStringLiteral("z"), QStringLiteral("0")));
        }
        REQUIRE(model->setStepValue(1, QStringLiteral("width"), QStringLiteral("30")));
        model->waitForCalculation();
        viewport.setProperty("animated", false);
        viewport.setWidth(1000.0);
        viewport.setHeight(600.0);
        viewport.setCamera(35.0, 30.0, 260.0);
        viewport.setScene(model->scene());
        selection = std::make_unique<SelectionController>(model.get());
        selection->attach(&viewport);
        align = std::make_unique<AlignController>(model.get(), selection.get());
        align->attach(&viewport);
        align->setEnabled(true);
    }

    void chooseAll() {
        selection->chooseBodies({model->bodyOfStep(0), model->bodyOfStep(1), model->bodyOfStep(2)});
        align->refresh();
    }
    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
        align->refresh();
    }
    double value(int step, const char* name) {
        model->selectStep(step);
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap()["name"].toString() == QLatin1String(name)) {
                return entry.toMap()["value"].toDouble();
            }
        }
        return -1e9;
    }
    double low(int step) {
        const auto bounds = model->bodyBounds(model->bodyOfStep(step));
        REQUIRE(bounds.has_value());
        return bounds->min.x;
    }
    double high(int step) {
        const auto bounds = model->bodyBounds(model->bodyOfStep(step));
        REQUIRE(bounds.has_value());
        return bounds->max.x;
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    Viewport3D viewport;
    std::unique_ptr<SelectionController> selection;
    std::unique_ptr<AlignController> align;
};

} // namespace

TEST_CASE("align: nine points show with two or more bodies chosen", "[ui][align]") {
    Fixture f;
    CHECK(f.align->points().isEmpty()); // nothing chosen
    f.selection->chooseBodies({f.model->bodyOfStep(0)});
    f.align->refresh();
    CHECK(f.align->points().isEmpty()); // one body: nothing to line up with
    f.chooseAll();
    REQUIRE(f.align->points().size() == 9);
    const auto& drawn = f.viewport.handleGraphics(Viewport3D::HandleSource::align);
    CHECK(drawn.dots.size() == 9);
    CHECK(drawn.lines.size() == 3);
    for (const QVariant& entry : f.align->points()) {
        CHECK(entry.toMap()["x"].toDouble() > 0.0);
        CHECK(entry.toMap()["x"].toDouble() < 1000.0);
    }
    f.align->setEnabled(false); // another tool
    CHECK(f.align->points().isEmpty());
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::align).empty());
}

TEST_CASE("align: pointing at a point shows the plane the bodies line up on", "[ui][align]") {
    Fixture f;
    f.chooseAll();
    const int left = f.align->indexOf(QStringLiteral("x"), QStringLiteral("min"));
    REQUIRE(left >= 0);
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::align).quads.empty());
    f.align->setPointed(left, true);
    const auto& drawn = f.viewport.handleGraphics(Viewport3D::HandleSource::align);
    REQUIRE(drawn.quads.size() == 1);
    // The plane stands at the lowest x of all three bodies (-60), the same for its four corners.
    for (const auto& corner : drawn.quads[0].corners) {
        CHECK(corner.x() == Approx(-60.0).margin(1e-3));
    }
    f.align->setPointed(left, false);
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::align).quads.empty());
}

TEST_CASE("align: a click puts all chosen bodies flush on the left", "[ui][align]") {
    Fixture f;
    f.chooseAll();
    const int before = f.model->historyPosition();
    REQUIRE(f.align->align(f.align->indexOf(QStringLiteral("x"), QStringLiteral("min"))));
    f.refresh();
    for (const int step : {0, 1, 2}) {
        CHECK(f.low(step) == Approx(-60.0).margin(1e-6));
    }
    CHECK(f.model->historyPosition() == before + 1); // one click, one step
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("alignBodies"));
    CHECK(f.value(1, "y") == Approx(0.0));
    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.low(0) == Approx(-10.0).margin(1e-6));
}

TEST_CASE("align: middle and right, in X and in Y", "[ui][align]") {
    Fixture f;
    f.chooseAll();
    REQUIRE(f.align->align(f.align->indexOf(QStringLiteral("x"), QStringLiteral("max"))));
    f.refresh();
    for (const int step : {0, 1, 2}) {
        CHECK(f.high(step) == Approx(75.0).margin(1e-6));
    }
    REQUIRE(f.model->setStepValue(2, QStringLiteral("y"), QStringLiteral("40")));
    f.refresh();
    f.chooseAll();
    REQUIRE(f.align->align(f.align->indexOf(QStringLiteral("y"), QStringLiteral("centre"))));
    f.refresh();
    const double y0 = f.value(0, "y");
    CHECK(f.value(1, "y") == Approx(y0));
    CHECK(f.value(2, "y") == Approx(y0));
}

TEST_CASE("align: a formula in the other directions stays", "[ui][align]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(1, QStringLiteral("y"), QStringLiteral("5 + 5")));
    f.refresh();
    f.chooseAll();
    REQUIRE(f.align->align(f.align->indexOf(QStringLiteral("x"), QStringLiteral("min"))));
    f.refresh();
    f.model->selectStep(1);
    QString expression;
    for (const QVariant& entry : f.model->stepValues()) {
        if (entry.toMap()["name"].toString() == QLatin1String("y")) {
            expression = entry.toMap()["expression"].toString();
        }
    }
    CHECK(expression == QStringLiteral("5 + 5"));
}

TEST_CASE("align: a click on a point that is not there does nothing", "[ui][align]") {
    Fixture f;
    CHECK_FALSE(f.align->align(0)); // nothing chosen
    f.chooseAll();
    CHECK_FALSE(f.align->align(42));
    CHECK_FALSE(f.align->align(-1));
}
