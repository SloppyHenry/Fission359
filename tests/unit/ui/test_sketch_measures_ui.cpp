// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Rules and measures of a sketch through the document model (AP-2.2b): the model owns the one
// solver, adds and changes rules and measures with the core commands and the sketch is solved
// again; undo brings the old value back, a formula that does not parse changes nothing. No
// window: the clicks are lines of sight straight down onto the ground plane.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QStringList>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/geo/sketch/planegcs_solver.h"
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

// Pointer snapping is off for the tests that put points at odd numbers.
struct NoCatching {
    NoCatching() { fission::ui::setSnappingSuppressedForTests(true); }
    ~NoCatching() { fission::ui::setSnappingSuppressedForTests(false); }
};

struct Drawing {
    Drawing() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel, nullptr,
                                                fission::geo::sketch::makePlaneGcsSolver());
        selection = std::make_unique<SelectionController>(model.get());
        sketch = std::make_unique<SketchController>(model.get(), selection.get());
        QObject::connect(sketch.get(), &SketchController::notice, &holder,
                         [this](const QString& code) { notices << code; });
        sketch->start();
        REQUIRE(sketch->pickGround());
        settle();
        REQUIRE(sketch->planeReady());
    }

    void settle() {
        model->waitForCalculation();
        QCoreApplication::processEvents();
    }

    // The line of sight straight down onto the point (u, v) of the ground plane.
    static interaction::PlaneRay down(double u, double v) {
        return {{u, v, 100.0}, {0.0, 0.0, -1.0}};
    }
    void move(double u, double v) { sketch->pointAt(down(u, v), 2.0); }
    void click(double u, double v, bool alt = false) { sketch->clickAt(down(u, v), 2.0, alt); }

    std::vector<core::SketchElement> elements() const {
        return model->sketchElements(sketch->step());
    }
    QString nextUndo() const { return model->nextUndo().value("key").toString(); }

    std::shared_ptr<fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    std::unique_ptr<SelectionController> selection;
    std::unique_ptr<SketchController> sketch;
    QObject holder;
    QStringList notices;
};

void checkPoint(const core::SketchPoint& point, double u, double v) {
    CHECK(point.u == Approx(u).margin(1e-9));
    CHECK(point.v == Approx(v).margin(1e-9));
}

} // namespace

TEST_CASE("sketch measures: rules and measures through the model solve the sketch",
          "[ui][sketchmeasures]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 0);
    REQUIRE(f.elements().size() == 1);
    CHECK(f.elements()[0].id == "L1");
    const QString step = f.sketch->step();
    CHECK(f.model->sketchSolver() != nullptr);
    CHECK(f.model->evaluateSketchValue(QStringLiteral("2 * 20"), false).value_or(0.0) ==
          Approx(40.0));
    CHECK_FALSE(f.model->evaluateSketchValue(QStringLiteral("abc"), false).has_value());

    // a) Fix the start point, make the line horizontal and give it a length of 30 mm: the end
    //    point moves to 30 mm from the start, on the horizontal through it.
    CHECK(f.model->addSketchRule(step, QStringLiteral("fixed"), {QStringLiteral("L1.0")}));
    CHECK(f.model->addSketchRule(step, QStringLiteral("horizontal"), {QStringLiteral("L1")}));
    CHECK(f.model->addSketchMeasure(step, QStringLiteral("length"), {QStringLiteral("L1")},
                                    QStringLiteral("30")));
    f.settle();
    auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[0], 0, 0);  // the fixed start stays where it was
    checkPoint(elements[0].points[1], 30, 0); // horizontal, 30 mm from the start

    // b) The measure (the third "R" id: two rules came first) takes a new formula; undo brings
    //    the old value back.
    CHECK(f.model->setSketchMeasure(step, QStringLiteral("R3"), QStringLiteral("2 * 20")));
    f.settle();
    elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[1], 40, 0); // 2 * 20 mm long now
    REQUIRE(f.model->undo());
    f.settle();
    elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[1], 30, 0); // back to 30 mm

    // c) A formula that does not parse is refused and changes nothing.
    const int position = f.model->historyPosition();
    CHECK_FALSE(f.model->addSketchMeasure(step, QStringLiteral("length"), {QStringLiteral("L1")},
                                          QStringLiteral("abc")));
    f.settle();
    CHECK(f.model->historyPosition() == position);
    elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[1], 30, 0);

    // A rule can be removed again (no solving, the coordinates stay), and undo brings it back.
    CHECK(f.model->removeSketchConstraint(step, QStringLiteral("R2")));
    CHECK(f.nextUndo() == "removeSketchConstraint");
    REQUIRE(f.model->undo());
    f.settle();
    elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[1], 30, 0);
}
