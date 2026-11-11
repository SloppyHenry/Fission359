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
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
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

TEST_CASE("sketch measures: choosing elements and setting a measure through the controller",
          "[ui][sketchmeasures]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 0);
    REQUIRE(f.elements().size() == 1);
    f.sketch->chooseTool(QString()); // no tool: a click chooses an element

    // a) A click on the middle of the drawn line chooses it; the measure kind is a length.
    f.click(5, 0);
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L1")});
    CHECK(f.sketch->measureKindForSelection() == QStringLiteral("length"));

    // b) A measure for the choice: one label with the typed value as its text.
    CHECK(f.sketch->setMeasureForSelection(QStringLiteral("30")));
    f.settle();
    QVariantList labels = f.sketch->measureLabels();
    REQUIRE(labels.size() == 1);
    const QVariantMap label = labels.first().toMap();
    CHECK(label.value(QStringLiteral("id")).toString() == QStringLiteral("R1"));
    CHECK(label.value(QStringLiteral("kind")).toString() == QStringLiteral("length"));
    CHECK(label.value(QStringLiteral("expression")).toString() == QStringLiteral("30"));
    CHECK(label.value(QStringLiteral("text")).toString() == QStringLiteral("30"));
    CHECK_FALSE(label.value(QStringLiteral("conflict")).toBool());
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L1")}); // the choice stays

    // c) The measure takes a new value; the label follows.
    CHECK(f.sketch->changeMeasure(QStringLiteral("R1"), QStringLiteral("45")));
    f.settle();
    CHECK(f.sketch->measureLabels().first().toMap().value(QStringLiteral("text")).toString() ==
          QStringLiteral("45"));

    // d) Esc lets go of the choice, a click on empty space as well.
    f.sketch->escape();
    CHECK(f.sketch->selectedElements().isEmpty());
    f.click(5, 0); // still on the line (the measure made it 45 mm long)
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L1")});
    f.click(60, 60);
    CHECK(f.sketch->selectedElements().isEmpty());

    // e) Shift adds to the choice: two lines give an angle, a third drops the oldest.
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 20);
    f.click(10, 25); // L2, not parallel to L1
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 40);
    f.click(10, 45); // L3, parallel to L2
    f.sketch->chooseTool(QString());
    f.sketch->clickAt(Drawing::down(5, 0), 2.0, false, true);    // Shift on L1
    f.sketch->clickAt(Drawing::down(5, 22.5), 2.0, false, true); // Shift on L2
    CHECK(f.sketch->selectedElements() ==
          QStringList({QStringLiteral("L1"), QStringLiteral("L2")}));
    CHECK(f.sketch->measureKindForSelection() == QStringLiteral("angle"));
    f.sketch->clickAt(Drawing::down(5, 42.5), 2.0, false, true); // Shift on L3: L1 goes
    CHECK(f.sketch->selectedElements() ==
          QStringList({QStringLiteral("L2"), QStringLiteral("L3")}));
    CHECK(f.sketch->measureKindForSelection() == QStringLiteral("distance")); // parallel
    f.sketch->clickAt(Drawing::down(5, 42.5), 2.0, false, true); // Shift on L3 again: it goes
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L2")});
    f.sketch->escape();
    CHECK(f.sketch->selectedElements().isEmpty());

    // f) A circle: the measure kind is the diameter and the label carries the Ø.
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(20, 20);
    f.click(25, 20); // middle and a point on the rim: radius 5
    REQUIRE(f.elements().size() == 4);
    f.sketch->chooseTool(QString());
    f.click(25, 20); // on the rim
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("C1")});
    CHECK(f.sketch->measureKindForSelection() == QStringLiteral("diameter"));
    CHECK(f.sketch->setMeasureForSelection(QStringLiteral("12")));
    f.settle();
    labels = f.sketch->measureLabels();
    REQUIRE(labels.size() == 2);
    const QVariantMap circleLabel = labels.last().toMap();
    CHECK(circleLabel.value(QStringLiteral("kind")).toString() == QStringLiteral("diameter"));
    CHECK(circleLabel.value(QStringLiteral("text")).toString() == QString::fromUtf8("Ø 12"));

    // g) The measure can be removed again through the controller.
    CHECK(f.sketch->removeMeasure(circleLabel.value(QStringLiteral("id")).toString()));
    f.settle();
    CHECK(f.sketch->measureLabels().size() == 1);
}

TEST_CASE("sketch rules: the rules that fit the chosen elements through the controller",
          "[ui][sketchmeasures]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 4); // L1, not horizontal
    REQUIRE(f.elements().size() == 1);
    f.sketch->chooseTool(QString()); // no tool: a click chooses an element

    // a) One line chosen: horizontal, vertical and fixed, in this order.
    f.click(5, 2);
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L1")});
    CHECK(f.sketch->rulesForSelection() ==
          QStringList(
              {QStringLiteral("horizontal"), QStringLiteral("vertical"), QStringLiteral("fixed")}));

    // b) "vertical" is taken and the line stands upright: both points at the same u.
    CHECK(f.sketch->addRuleForSelection(QStringLiteral("vertical")));
    f.settle();
    auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    CHECK(elements[0].points[0].u == Approx(elements[0].points[1].u).margin(1e-9));
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L1")}); // the choice stays

    // c) Two lines chosen: "parallel" fits, "horizontal" does not.
    f.sketch->escape();
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 20);
    f.click(8, 24); // L2, not parallel to the now vertical L1
    REQUIRE(f.elements().size() == 2);
    f.sketch->chooseTool(QString());
    const auto middleRay = [](const core::SketchElement& e) {
        return Drawing::down((e.points[0].u + e.points[1].u) / 2.0,
                             (e.points[0].v + e.points[1].v) / 2.0);
    };
    const core::SketchElement* line1 = nullptr;
    const core::SketchElement* line2 = nullptr;
    const auto drawn = f.elements(); // a copy that lives as long as the pointers into it
    for (const core::SketchElement& e : drawn) {
        if (e.id == "L1") {
            line1 = &e;
        } else if (e.id == "L2") {
            line2 = &e;
        }
    }
    REQUIRE(line1 != nullptr);
    REQUIRE(line2 != nullptr);
    f.sketch->clickAt(middleRay(*line1), 2.0, false, true); // Shift on L1
    f.sketch->clickAt(middleRay(*line2), 2.0, false, true); // Shift on L2
    CHECK(f.sketch->selectedElements() ==
          QStringList({QStringLiteral("L1"), QStringLiteral("L2")}));
    CHECK(f.sketch->rulesForSelection().contains(QStringLiteral("parallel")));
    CHECK_FALSE(f.sketch->addRuleForSelection(QStringLiteral("horizontal")));

    // d) "parallel" is taken: the two directions cross at zero.
    CHECK(f.sketch->addRuleForSelection(QStringLiteral("parallel")));
    f.settle();
    line1 = nullptr;
    line2 = nullptr;
    const auto solved = f.elements(); // a copy that lives as long as the pointers into it
    for (const core::SketchElement& e : solved) {
        if (e.id == "L1") {
            line1 = &e;
        } else if (e.id == "L2") {
            line2 = &e;
        }
    }
    REQUIRE(line1 != nullptr);
    REQUIRE(line2 != nullptr);
    const double ax = line1->points[1].u - line1->points[0].u;
    const double ay = line1->points[1].v - line1->points[0].v;
    const double bx = line2->points[1].u - line2->points[0].u;
    const double by = line2->points[1].v - line2->points[0].v;
    CHECK(std::abs(ax * by - ay * bx) == Approx(0.0).margin(1e-9));
}

TEST_CASE("free elements are known and get fixed by measures", "[ui][sketchmeasures][dof]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 0);
    REQUIRE(f.elements().size() == 1);
    f.settle();

    // No rules and no measures yet: the one line can still move (AP-2.2c1).
    CHECK(f.sketch->freeCount() == 1);
    CHECK_FALSE(f.sketch->fullyFixed());

    // A "fixed" rule on the chosen line: nothing can move any more.
    f.sketch->chooseTool(QString()); // no tool: a click chooses an element
    f.click(5, 0);
    CHECK(f.sketch->selectedElements() == QStringList{QStringLiteral("L1")});
    CHECK(f.sketch->addRuleForSelection(QStringLiteral("fixed")));
    f.settle();
    CHECK(f.sketch->freeCount() == 0);
    CHECK(f.sketch->fullyFixed());
}
