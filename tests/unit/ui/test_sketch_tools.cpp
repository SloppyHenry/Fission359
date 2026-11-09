// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The drawing tools of the sketch mode (AP-2.1c1) through the controller: a series of clicks makes
// exactly the expected elements in the document, one undo step each, Esc ends the element, then
// the tool, then the sketch; typed numbers, helper lines, too small elements. No window: the
// clicks are lines of sight straight down onto the ground plane.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <chrono>
#include <cmath>
#include <memory>
#include <numbers>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/numbers.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/sketch_controller.h"
#include "fission/ui/snap_session.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::Numbers;
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
        model = std::make_unique<DocumentModel>(kernel);
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
    std::size_t count(core::SketchElementKind kind) const {
        std::size_t n = 0;
        for (const core::SketchElement& element : elements()) {
            n += element.kind == kind ? 1 : 0;
        }
        return n;
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

TEST_CASE("sketch tools: choosing a tool works only inside a sketch", "[ui][sketchtools]") {
    application();
    std::shared_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    DocumentModel model(kernel);
    SelectionController selection(&model);
    SketchController sketch(&model, &selection);
    sketch.chooseTool(QStringLiteral("line"));
    CHECK_FALSE(sketch.toolActive());

    Drawing f;
    CHECK_FALSE(f.sketch->toolActive());
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    CHECK(f.sketch->toolActive());
    CHECK(f.sketch->toolName() == "rectangle");
    CHECK(f.sketch->toolVariant() == 0); // the first variant is the standard
    f.sketch->chooseTool(QStringLiteral("rectangle"), 1);
    CHECK(f.sketch->toolVariant() == 1);
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.sketch->chooseTool(QStringLiteral("rectangle")); // remembers the variant of each tool
    CHECK(f.sketch->toolVariant() == 1);
    f.sketch->chooseTool(QString());
    CHECK_FALSE(f.sketch->toolActive());
}

TEST_CASE("sketch tools: a chain of lines makes one line per click, each one undo step",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    CHECK(f.elements().empty());
    CHECK(f.sketch->drawing());
    f.click(12.5, 0);
    f.click(12.5, 7.25);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    CHECK(elements[0].id == "L1");
    CHECK(elements[1].id == "L2");
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[0].points[1], 12.5, 0);
    CHECK(elements[1].points[0] == elements[0].points[1]); // exactly the same coordinates
    checkPoint(elements[1].points[1], 12.5, 7.25);
    CHECK(f.sketch->toolActive()); // the tool stays on
    CHECK(f.nextUndo() == "addSketchElement");

    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().size() == 1);
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().empty());
}

TEST_CASE("sketch tools: a double click ends the chain, the next click starts a new one",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 0);
    f.click(10, 10);         // the first click of the double click ...
    f.sketch->doubleClick(); // ... and the double click itself
    CHECK_FALSE(f.sketch->drawing());
    CHECK(f.elements().size() == 2);
    f.click(30, 30); // a new chain: no line yet
    CHECK(f.elements().size() == 2);
    CHECK(f.sketch->drawing());
    f.click(40, 30);
    CHECK(f.elements().size() == 3);
    CHECK(f.elements()[2].points[0] == core::SketchPoint{30, 30});
}

TEST_CASE("sketch tools: Esc ends the element, then the tool, then the sketch",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 0);
    REQUIRE(f.sketch->drawing());

    f.sketch->escape(); // the chain ends, the tool stays
    CHECK_FALSE(f.sketch->drawing());
    CHECK(f.sketch->toolActive());
    CHECK(f.sketch->active());
    CHECK(f.elements().size() == 1); // what was drawn stays

    f.sketch->escape(); // the tool ends
    CHECK_FALSE(f.sketch->toolActive());
    CHECK(f.sketch->active());

    f.sketch->escape(); // the sketch ends; it has a line, so it stays
    CHECK_FALSE(f.sketch->engaged());
    f.settle();
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("sketch tools: a rectangle is four lines and one undo step", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(5, 5);
    CHECK(f.elements().empty());
    f.click(35.5, 25);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 4);
    for (const auto& line : elements) {
        CHECK(line.kind == core::SketchElementKind::line);
    }
    CHECK(elements[0].id == "L1");
    CHECK(elements[3].id == "L4");
    checkPoint(elements[0].points[0], 5, 5);
    checkPoint(elements[0].points[1], 35.5, 5);
    checkPoint(elements[1].points[1], 35.5, 25);
    checkPoint(elements[2].points[1], 5, 25);
    checkPoint(elements[3].points[1], 5, 5);
    CHECK(f.sketch->toolActive());
    CHECK(f.nextUndo() == "addSketchElement");

    REQUIRE(f.model->undo()); // one step takes all four sides
    f.settle();
    CHECK(f.elements().empty());
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.elements().size() == 4);
    CHECK(f.elements()[0].id == "L1"); // redo gives the same ids
}

TEST_CASE("sketch tools: a rectangle by its middle and a corner", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("rectangle"), 1);
    f.click(10, 10);
    f.click(16, 14);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 4);
    checkPoint(elements[0].points[0], 4, 6);
    checkPoint(elements[2].points[0], 16, 14);
}

TEST_CASE("sketch tools: a circle by its middle and a point on it", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(20, 20);
    f.move(20, 26);
    CHECK(f.sketch->readout().value("valid").toBool());
    f.click(20, 29);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    CHECK(elements[0].kind == core::SketchElementKind::circle);
    CHECK(elements[0].id == "C1");
    checkPoint(elements[0].points[0], 20, 20);
    checkPoint(elements[0].points[1], 20, 29);
    CHECK_FALSE(f.sketch->drawing());
}

TEST_CASE("sketch tools: a circle through three points", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("circle"), 1);
    f.click(10, 0);
    f.click(0, 10);
    CHECK(f.elements().empty());
    f.click(-10, 0);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[0], 0, 0);  // the middle
    checkPoint(elements[0].points[1], 10, 0); // the first point lies on it
    CHECK(f.notices.isEmpty());
}

TEST_CASE("sketch tools: three points on a line make no circle and say so in the hint line",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("circle"), 1);
    f.click(0, 0);
    f.click(10, 10);
    f.click(20, 20);
    CHECK(f.elements().empty());
    REQUIRE(f.notices.size() == 1);
    CHECK(f.notices[0] == "collinear");
    CHECK(f.model->recipe().size() == 1); // no dialog, no step: only the sketch
    f.click(0, 20);                       // another third point works
    CHECK(f.elements().size() == 1);
}

TEST_CASE("sketch tools: the point tool places a point", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("point"));
    f.click(3, 4);
    f.click(7, 8);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    CHECK(elements[0].kind == core::SketchElementKind::point);
    CHECK(elements[0].id == "P1");
    checkPoint(elements[1].points[0], 7, 8);
    CHECK(f.nextUndo() == "addSketchElement");
}

TEST_CASE("sketch tools: elements shorter than 0.01 mm are not drawn", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(0.004, 0);
    CHECK(f.elements().empty());
    REQUIRE(f.notices.size() == 1);
    CHECK(f.notices[0] == "tooSmall");
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(5, 5);
    f.click(5.002, 5);
    CHECK(f.elements().empty());
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(5, 5);
    f.click(9, 5.001);
    CHECK(f.elements().empty());
    CHECK(f.notices.size() == 3);
}

TEST_CASE("sketch tools: the numbers of a line follow the pointer", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    CHECK_FALSE(f.sketch->readout().value("valid").toBool());
    f.click(0, 0);
    f.move(3, 4);
    const QVariantMap readout = f.sketch->readout();
    REQUIRE(readout.value("valid").toBool());
    const QVariantList fields = readout.value("fields").toList();
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].toMap().value("name").toString() == "length");
    CHECK(fields[0].toMap().value("value").toDouble() == Approx(5.0));
    CHECK(fields[1].toMap().value("name").toString() == "angle");
    CHECK(fields[1].toMap().value("value").toDouble() == Approx(53.130102));
    CHECK(f.sketch->fieldCount() == 2);
}

TEST_CASE("sketch tools: typing a length and an angle sets the line, Enter accepts it",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(10, 10);
    f.move(15, 10);
    f.sketch->setTyped(0, 30.0);
    f.sketch->setTyped(1, 90.0);
    f.sketch->acceptTyped();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[0], 10, 10);
    checkPoint(elements[0].points[1], 10, 40);
    CHECK(f.sketch->drawing()); // the chain goes on from the end
    CHECK(f.nextUndo() == "addSketchElement");
}

TEST_CASE("sketch tools: typing a width and a height sets the rectangle", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.move(4, 4);
    f.sketch->setTyped(0, 40.0);
    f.sketch->setTyped(1, 25.0);
    f.sketch->acceptTyped();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 4);
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[2].points[0], 40, 25);
}

TEST_CASE("sketch tools: typing the radius sets the circle, as for a cylinder or a sphere",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(0, 0);
    f.move(3, 0);
    f.sketch->setTyped(0, 15.0);
    f.sketch->acceptTyped();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[0].points[1], 15, 0);
}

TEST_CASE("sketch tools: a typed expression with a unit is understood", "[ui][sketchtools]") {
    NoCatching free;
    Numbers numbers;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.move(1, 0);
    // what the number field does with the text before it hands the number over
    f.sketch->setTyped(0, numbers.evaluate(QStringLiteral("2,5 cm + 5"), QStringLiteral("length")));
    f.sketch->setTyped(1, numbers.evaluate(QStringLiteral("90"), QStringLiteral("angle")));
    f.sketch->acceptTyped();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    checkPoint(elements[0].points[1], 0, 30);
}

TEST_CASE("sketch tools: taking the typed number back lets the pointer decide again",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.move(8, 0);
    f.sketch->setTyped(0, 50.0);
    f.sketch->clearTyped();
    f.sketch->acceptTyped();
    REQUIRE(f.elements().size() == 1);
    checkPoint(f.elements()[0].points[1], 8, 0);
    f.sketch->setTyped(0, std::nan("")); // NaN takes it back, too
}

TEST_CASE("sketch tools: nothing to type before the first click or for three points",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    CHECK(f.sketch->fieldCount() == 0);
    f.sketch->setTyped(0, 5.0);
    f.sketch->acceptTyped();
    CHECK(f.elements().empty());
    f.sketch->chooseTool(QStringLiteral("circle"), 1);
    f.click(0, 0);
    CHECK(f.sketch->fieldCount() == 0);
}

TEST_CASE("sketch tools: the helper line switch makes aids, Alt does for one element",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(10, 0);
    CHECK_FALSE(f.elements()[0].construction);

    f.sketch->toggleHelperLine();
    CHECK(f.sketch->helperLine());
    f.click(10, 10);
    CHECK(f.elements()[1].construction);
    f.sketch->toggleHelperLine();
    CHECK_FALSE(f.sketch->helperLine());
    f.click(20, 10);
    CHECK_FALSE(f.elements()[2].construction);
    f.click(30, 10, /*alt=*/true);
    CHECK(f.elements()[3].construction);
    f.click(40, 10);
    CHECK_FALSE(f.elements()[4].construction);

    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(0, 50);
    f.click(5, 50, /*alt=*/true);
    CHECK(f.elements().back().kind == core::SketchElementKind::circle);
    CHECK(f.elements().back().construction);
}

TEST_CASE("sketch tools: the helper line switch does not outlive the sketch", "[ui][sketchtools]") {
    Drawing f;
    f.sketch->toggleHelperLine();
    REQUIRE(f.sketch->helperLine());
    f.sketch->finish();
    CHECK_FALSE(f.sketch->helperLine());
    CHECK_FALSE(f.sketch->toolActive());
}

TEST_CASE("sketch tools: the pointer catches on the points of the sketch while drawing",
          "[ui][sketchtools]") {
    Drawing f; // catching is on: the grid is 5 mm here, points win over it
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(20, 0);
    f.sketch->escape();
    f.click(20.9, 0.6); // near the end of the first line: it catches on it
    f.click(20.4, 30.2);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    CHECK(elements[1].points[0] == elements[0].points[1]);
}

TEST_CASE("sketch tools: clicks without a tool draw nothing", "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.click(5, 5);
    f.click(15, 15);
    CHECK(f.elements().empty());
    CHECK_FALSE(f.sketch->drawing());
}

TEST_CASE("sketch tools: a rectangle and a circle in a sketch keep their ids apart",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("rectangle"));
    f.click(0, 0);
    f.click(10, 10);
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(5, 5);
    f.click(8, 5);
    CHECK(f.count(core::SketchElementKind::line) == 4);
    CHECK(f.count(core::SketchElementKind::circle) == 1);
    const auto elements = f.elements();
    CHECK(elements.back().id == "C1");
    // the sketch with its elements survives leaving it
    f.sketch->finish();
    f.settle();
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("sketch tools: after a typed value Esc still ends the tool and then the sketch",
          "[ui][sketchtools]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("circle"));
    f.click(0, 0);
    f.move(3, 0);
    f.sketch->setTyped(0, 12.5);
    f.sketch->acceptTyped();
    REQUIRE(f.elements().size() == 1);
    f.sketch->escape(); // nothing is being drawn: the tool ends
    CHECK_FALSE(f.sketch->toolActive());
    CHECK(f.sketch->active());
    f.sketch->escape(); // the sketch ends (it has a circle, so it stays)
    CHECK_FALSE(f.sketch->engaged());
    f.settle();
    CHECK(f.model->recipe().size() == 1);
}

// --- AP-2.1c2: arc, tangential arc, slot, polygon --------------------------------------------

TEST_CASE("sketch tools: an arc by three points is one element, one undo step",
          "[ui][sketchtools][arc]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("arc"));
    CHECK(f.sketch->toolName() == "arc");
    f.click(10, 0);
    f.click(-10, 0);
    CHECK(f.elements().empty());
    f.click(0, 10); // counter-clockwise from the first to the second point
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    CHECK(elements[0].kind == core::SketchElementKind::arc);
    CHECK(elements[0].id == "A1");
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[0].points[1], 10, 0);
    checkPoint(elements[0].points[2], -10, 0);
    CHECK(f.nextUndo() == "addSketchElement");
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().empty());
}

TEST_CASE("sketch tools: an arc drawn clockwise has start and end swapped, collinear points say so",
          "[ui][sketchtools][arc]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("arc"));
    f.click(-10, 0);
    f.click(10, 0);
    f.click(0, 10);
    REQUIRE(f.elements().size() == 1);
    checkPoint(f.elements()[0].points[1], 10, 0);
    checkPoint(f.elements()[0].points[2], -10, 0);

    f.click(0, 0);
    f.click(10, 0);
    f.click(5, 0);
    CHECK(f.notices.contains("collinear"));
    CHECK(f.elements().size() == 1);
    CHECK(f.sketch->placed() == 2);
    f.click(5, -3);
    CHECK(f.elements().size() == 2);
}

TEST_CASE("sketch tools: an arc with a typed radius smaller than half the chord says too small",
          "[ui][sketchtools][arc]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("arc"));
    f.click(0, 0);
    f.click(10, 0);
    f.move(5, 4);
    CHECK(f.sketch->fieldCount() == 1);
    f.sketch->setTyped(0, 4.0);
    f.sketch->acceptTyped();
    CHECK(f.notices.contains("tooSmall"));
    CHECK(f.elements().empty());
    f.sketch->setTyped(0, 6.5);
    f.sketch->acceptTyped();
    REQUIRE(f.elements().size() == 1);
    CHECK(std::hypot(f.elements()[0].points[1].u - f.elements()[0].points[0].u,
                     f.elements()[0].points[1].v - f.elements()[0].points[0].v) == Approx(6.5));
}

TEST_CASE("sketch tools: an arc by middle, start and end, with a typed angle",
          "[ui][sketchtools][arc]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("arc"), 1);
    CHECK(f.sketch->toolVariant() == 1);
    f.click(0, 0);
    f.click(10, 0);
    f.move(0, 10);
    CHECK(f.sketch->readout().value("fields").toList().at(0).toMap().value("name") == "angle");
    f.sketch->setTyped(0, 120.0);
    f.sketch->acceptTyped();
    REQUIRE(f.elements().size() == 1);
    const auto arc = f.elements()[0];
    checkPoint(arc.points[0], 0, 0);
    checkPoint(arc.points[1], 10, 0);
    checkPoint(arc.points[2], 10 * std::cos(2.0 * std::numbers::pi / 3.0),
               10 * std::sin(2.0 * std::numbers::pi / 3.0));
}

TEST_CASE("sketch tools: pulling on the end of a line makes a tangential arc, the chain goes on",
          "[ui][sketchtools][tangent]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(20, 0);
    REQUIRE(f.elements().size() == 1);
    f.sketch->pressAt(Drawing::down(20, 0), 2.0);
    CHECK_FALSE(f.sketch->pullingArc());
    f.sketch->dragTo(Drawing::down(21, 1), 2.0); // under 6 screen pixels (3 mm here)
    CHECK_FALSE(f.sketch->pullingArc());
    f.sketch->dragTo(Drawing::down(30, 10), 2.0);
    CHECK(f.sketch->pullingArc());
    CHECK(f.sketch->readout().value("fields").toList().at(0).toMap().value("name") == "radius");
    CHECK(f.elements().size() == 1); // nothing yet: a preview
    f.sketch->releaseAt(Drawing::down(30, 10), 2.0);
    CHECK_FALSE(f.sketch->pullingArc());
    const auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    CHECK(elements[1].kind == core::SketchElementKind::arc);
    checkPoint(elements[1].points[0], 20, 10);
    checkPoint(elements[1].points[1], 20, 0); // starts exactly where the line ended
    CHECK(elements[1].points[1] == elements[0].points[1]);
    checkPoint(elements[1].points[2], 30, 10);
    // the arc starts in the direction of the line, on 1e-9
    const double ru = elements[1].points[1].u - elements[1].points[0].u;
    const double rv = elements[1].points[1].v - elements[1].points[0].v;
    CHECK(-rv / std::hypot(ru, rv) == Approx(1.0).margin(1e-9));
    CHECK(f.nextUndo() == "addSketchElement");

    // the chain goes on from the end of the arc as a line
    f.click(30, 30);
    REQUIRE(f.elements().size() == 3);
    CHECK(f.elements()[2].kind == core::SketchElementKind::line);
    CHECK(f.elements()[2].points[0] == f.elements()[1].points[2]);

    REQUIRE(f.model->undo()); // the line
    REQUIRE(f.model->undo()); // the arc: one step
    f.settle();
    CHECK(f.elements().size() == 1);
}

TEST_CASE("sketch tools: a tangential arc needs a line before it, and Esc drops only the arc",
          "[ui][sketchtools][tangent]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.sketch->pressAt(Drawing::down(0, 0), 2.0); // no line yet
    f.sketch->dragTo(Drawing::down(10, 10), 2.0);
    CHECK_FALSE(f.sketch->pullingArc());
    f.sketch->releaseAt(Drawing::down(10, 10), 2.0);
    CHECK(f.elements().empty());

    f.click(20, 0);
    f.sketch->pressAt(Drawing::down(8, 0), 2.0); // not on the end
    f.sketch->dragTo(Drawing::down(10, 10), 2.0);
    CHECK_FALSE(f.sketch->pullingArc());
    f.sketch->releaseAt(Drawing::down(10, 10), 2.0);
    CHECK(f.elements().size() == 1);

    f.sketch->pressAt(Drawing::down(20, 0), 2.0);
    f.sketch->dragTo(Drawing::down(30, 10), 2.0);
    REQUIRE(f.sketch->pullingArc());
    f.sketch->escape();
    CHECK_FALSE(f.sketch->pullingArc());
    CHECK(f.sketch->drawing()); // the chain stays
    f.sketch->releaseAt(Drawing::down(30, 10), 2.0);
    CHECK(f.elements().size() == 1);
}

TEST_CASE("sketch tools: a tangential arc with a typed radius", "[ui][sketchtools][tangent]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.click(20, 0);
    f.sketch->pressAt(Drawing::down(20, 0), 2.0);
    f.sketch->dragTo(Drawing::down(40, 20), 2.0);
    REQUIRE(f.sketch->pullingArc());
    CHECK(f.sketch->fieldCount() == 1);
    f.sketch->setTyped(0, 5.0);
    f.sketch->acceptTyped();
    REQUIRE(f.elements().size() == 2);
    const auto arc = f.elements()[1];
    CHECK(std::hypot(arc.points[1].u - arc.points[0].u, arc.points[1].v - arc.points[0].v) ==
          Approx(5.0));
}

TEST_CASE("sketch tools: a slot is four elements with closed ends in one undo step",
          "[ui][sketchtools][slot]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("slot"));
    f.click(0, 0);
    f.click(20, 0);
    CHECK(f.elements().empty());
    f.move(10, 3);
    f.click(10, 3);
    const auto elements = f.elements();
    REQUIRE(elements.size() == 4);
    CHECK(f.count(core::SketchElementKind::line) == 2);
    CHECK(f.count(core::SketchElementKind::arc) == 2);
    // the ends are exactly the same points all round
    CHECK(elements[0].points[1] == elements[1].points[1]);
    CHECK(elements[1].points[2] == elements[2].points[0]);
    CHECK(elements[2].points[1] == elements[3].points[1]);
    CHECK(elements[3].points[2] == elements[0].points[0]);
    CHECK(std::hypot(elements[1].points[1].u - elements[1].points[0].u,
                     elements[1].points[1].v - elements[1].points[0].v) == Approx(3.0));
    CHECK(f.nextUndo() == "addSketchElement");
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().empty()); // one step took all four

    f.sketch->chooseTool(QStringLiteral("slot"));
    f.click(0, 0);
    f.move(5, 1);
    f.sketch->setTyped(0, 12.0);
    f.sketch->setTyped(1, 90.0);
    f.sketch->acceptTyped();
    f.sketch->setTyped(0, 6.0); // the whole width, not the radius
    f.sketch->acceptTyped();
    REQUIRE(f.elements().size() == 4);
    checkPoint(f.elements()[1].points[0], 0, 12);
    CHECK(std::hypot(f.elements()[1].points[1].u, f.elements()[1].points[1].v - 12) == Approx(3.0));
}

TEST_CASE("sketch tools: a polygon is n lines in one undo step in both variants",
          "[ui][sketchtools][polygon]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("polygon"));
    f.click(0, 0);
    f.click(10, 0);
    REQUIRE(f.elements().size() == 6);
    CHECK(f.count(core::SketchElementKind::line) == 6);
    CHECK(f.elements()[5].points[1] == f.elements()[0].points[0]);
    CHECK(f.nextUndo() == "addSketchElement");
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().empty());

    f.sketch->chooseTool(QStringLiteral("polygon"), 1); // by the middle of a side
    f.click(0, 0);
    f.move(0, 10);
    f.sketch->setTyped(1, 3.0);
    f.sketch->acceptTyped();
    CHECK(f.elements().size() == 3);
    f.sketch->chooseTool(QStringLiteral("polygon"), 0);
    f.click(50, 50);
    f.move(60, 50);
    f.sketch->setTyped(1, 64.0);
    f.sketch->acceptTyped();
    CHECK(f.elements().size() == 3 + 64);
}

TEST_CASE(
    "sketch tools: a wrong number of sides says so, the last number lasts until the sketch ends",
    "[ui][sketchtools][polygon]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("polygon"));
    f.click(0, 0);
    f.move(10, 0);
    f.sketch->setTyped(1, 2.0);
    CHECK(f.notices.contains("badSides"));
    f.sketch->setTyped(1, 5.0);
    f.sketch->acceptTyped();
    CHECK(f.elements().size() == 5);
    // the next polygon starts with five sides
    f.click(30, 0);
    f.move(40, 0);
    CHECK(f.sketch->readout().value("fields").toList().at(1).toMap().value("value").toInt() == 5);
    f.click(40, 0);
    CHECK(f.elements().size() == 10);

    // a new sketch starts with six
    f.sketch->finish();
    f.settle();
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    f.sketch->chooseTool(QStringLiteral("polygon"));
    f.click(0, 0);
    f.move(10, 0);
    CHECK(f.sketch->readout().value("fields").toList().at(1).toMap().value("value").toInt() == 6);
}

// --- the curve (AP-2.1d) ---------------------------------------------------------------------

TEST_CASE("sketch tools: a curve through clicked points is one element and one undo step",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    CHECK(f.sketch->toolName() == "spline");
    f.click(0, 0);
    f.click(10, 8);
    f.click(20, -4);
    f.click(35, 6);
    CHECK(f.elements().empty());
    CHECK(f.sketch->drawing());
    CHECK(f.sketch->placed() == 4);
    f.sketch->doubleClick();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 1);
    CHECK(elements[0].kind == core::SketchElementKind::spline);
    CHECK(elements[0].id == "S1");
    REQUIRE(elements[0].points.size() == 4);
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[0].points[3], 35, 6);
    CHECK(f.nextUndo() == "addSketchElement");
    CHECK_FALSE(f.sketch->drawing());
    CHECK(f.sketch->toolActive()); // the tool stays on for the next curve
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().empty());
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.elements().size() == 1);
}

TEST_CASE("sketch tools: Enter sets the curve, and a click on the first point closes it",
          "[ui][sketchtools][spline]") {
    Drawing f; // the pointer catches: the click near the first point lands on it
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(0, 0);
    f.click(20, 0);
    f.click(20, 20);
    f.sketch->acceptTyped(); // Enter
    REQUIRE(f.elements().size() == 1);
    CHECK(f.elements()[0].points.size() == 3);

    f.click(40, 0);
    f.click(60, 0);
    f.click(60, 20);
    f.click(40, 20);
    f.click(41, 1); // within the catching radius of the first point
    REQUIRE(f.elements().size() == 2);
    const auto closed = f.elements()[1];
    REQUIRE(closed.points.size() == 5);
    CHECK(closed.points.front() == closed.points.back()); // closed: the last point is the first
    CHECK_FALSE(f.sketch->drawing());
    CHECK(f.nextUndo() == "addSketchElement");
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().size() == 1); // the closed curve was one step
}

TEST_CASE("sketch tools: Esc throws away the unfinished curve, then ends the tool",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(0, 0);
    f.click(10, 10);
    f.click(20, 0);
    f.sketch->escape();
    CHECK_FALSE(f.sketch->drawing());
    CHECK(f.elements().empty());
    CHECK(f.sketch->toolActive());
    f.sketch->escape();
    CHECK_FALSE(f.sketch->toolActive());
    CHECK(f.sketch->active()); // the sketch itself is still open
}

TEST_CASE("sketch tools: Backspace takes the last point of the curve back",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(0, 0);
    f.click(10, 10);
    f.click(20, 0);
    f.sketch->removeLastPoint();
    CHECK(f.sketch->placed() == 2);
    f.click(15, -5);
    f.sketch->doubleClick();
    REQUIRE(f.elements().size() == 1);
    REQUIRE(f.elements()[0].points.size() == 3);
    checkPoint(f.elements()[0].points[2], 15, -5);
    // nothing to take back: nothing happens
    f.sketch->removeLastPoint();
    CHECK(f.elements().size() == 1);
    // other tools ignore it
    f.sketch->chooseTool(QStringLiteral("line"));
    f.click(0, 0);
    f.sketch->removeLastPoint();
    CHECK(f.sketch->drawing());
}

TEST_CASE("sketch tools: numbers typed for the curve make its next point",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(0, 0);
    f.move(5, 1);
    CHECK(f.sketch->fieldCount() == 2);
    f.sketch->setTyped(0, 12.0);
    f.sketch->setTyped(1, 90.0);
    f.sketch->acceptTyped();
    CHECK(f.sketch->placed() == 2);
    f.click(20, 12);
    f.sketch->acceptTyped(); // Enter sets it
    REQUIRE(f.elements().size() == 1);
    REQUIRE(f.elements()[0].points.size() == 3);
    checkPoint(f.elements()[0].points[1], 0, 12);
}

TEST_CASE(
    "sketch tools: a curve of one distinct point is not drawn, two points make a straight one",
    "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(5, 5);
    f.sketch->doubleClick();
    CHECK(f.elements().empty());
    CHECK(f.notices.contains("tooSmall"));
    CHECK_FALSE(f.sketch->drawing());

    f.click(0, 0);
    f.click(10, 0);
    f.sketch->doubleClick();
    REQUIRE(f.elements().size() == 1);
    CHECK(f.elements()[0].points.size() == 2);
}

TEST_CASE("sketch tools: the points of a curve are catching points afterwards",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(1.5, 2.5);
    f.click(11.5, 8.5);
    f.click(21.5, 2.5);
    f.sketch->doubleClick();
    REQUIRE(f.elements().size() == 1);
    fission::ui::setSnappingSuppressedForTests(false);
    f.sketch->chooseTool(QStringLiteral("point"));
    f.move(11.7, 8.6); // the middle point of the curve
    const QVariantMap snap = f.sketch->snap();
    CHECK(snap.value("kind").toString() == "point");
    CHECK(snap.value("element").toString() == "S1");
    CHECK(snap.value("u").toDouble() == Approx(11.5));
    fission::ui::setSnappingSuppressedForTests(true);
}

TEST_CASE("sketch tools: the helper line switch makes the curve an aid",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->setHelperLine(true);
    f.sketch->chooseTool(QStringLiteral("spline"));
    f.click(0, 0);
    f.click(10, 10);
    f.sketch->doubleClick();
    REQUIRE(f.elements().size() == 1);
    CHECK(f.elements()[0].construction);
}

TEST_CASE("sketch tools: the document never takes a curve the kernel refuses",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    // two equal points: the kernel finds no curve through them
    core::SketchElement bad;
    bad.kind = core::SketchElementKind::spline;
    bad.points = {{3, 3}, {3, 3}};
    CHECK_FALSE(f.model->sketchSplineLine(bad.points).has_value());
    CHECK_FALSE(f.model->addSketchElements(f.sketch->step(), {bad}, QStringLiteral("spline")));
    CHECK(f.elements().empty());
    CHECK(f.model->sketchSplineLine({{0, 0}, {10, 5}, {20, 0}}).has_value());
}

TEST_CASE("sketch tools: a curve of 50 points follows the pointer without lag",
          "[ui][sketchtools][spline]") {
    NoCatching free;
    Drawing f;
    f.sketch->chooseTool(QStringLiteral("spline"));
    for (int i = 0; i < 50; ++i) {
        f.click(i * 3.0, 20.0 * std::sin(i * 0.5));
    }
    REQUIRE(f.sketch->placed() == 50);
    constexpr int kMoves = 20;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kMoves; ++i) {
        f.move(150.0 + i, 5.0 + i); // each move calculates the curve afresh
    }
    const double each =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count() /
        kMoves;
    CHECK(each < 40.0);
    f.sketch->doubleClick();
    CHECK(f.elements().size() == 1);
    CHECK(f.elements()[0].points.size() == 50);
}

// --- trimming and extending (AP-2.1e1) ----------------------------------------------------

namespace {

core::SketchElement lineOf(double u1, double v1, double u2, double v2) {
    return {"", core::SketchElementKind::line, {{u1, v1}, {u2, v2}}, false};
}
core::SketchElement circleOf(double u, double v, double radius) {
    return {"", core::SketchElementKind::circle, {{u, v}, {u + radius, v}}, false};
}

// Puts elements into the sketch the way a tool would (each one a step), and lets the controller
// see them.
void put(Drawing& f, const std::vector<core::SketchElement>& elements) {
    for (const core::SketchElement& element : elements) {
        REQUIRE(f.model->addSketchElements(f.sketch->step(), {element}, QStringLiteral("line")));
    }
    f.settle();
}

} // namespace

TEST_CASE("sketch trim: a click on the piece between two crossings shortens the line",
          "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10), lineOf(30, -10, 30, 10)});
    f.sketch->chooseTool(QStringLiteral("trim"));
    CHECK(f.sketch->toolName() == "trim");
    CHECK_FALSE(f.sketch->drawing()); // nothing is pulled: no number to type

    f.move(35, 0.5);
    f.click(35, 0.5); // the end piece
    auto elements = f.elements();
    REQUIRE(elements.size() == 3);
    CHECK(elements[0].id == "L1");
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[0].points[1], 30, 0);
    CHECK(f.nextUndo() == "trimSketch");
    CHECK(f.notices.isEmpty());

    REQUIRE(f.model->undo());
    f.settle();
    elements = f.elements();
    REQUIRE(elements.size() == 3);
    checkPoint(elements[0].points[1], 40, 0);
}

TEST_CASE("sketch trim: a piece in the middle makes two lines, undo and redo give the same ids",
          "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10), lineOf(30, -10, 30, 10)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.click(20, 0.5);
    auto after = f.elements();
    REQUIRE(after.size() == 4);
    CHECK(after[0].id == "L1"); // the first piece keeps the id
    checkPoint(after[0].points[1], 10, 0);
    CHECK(after[3].id == "L4"); // the other piece is new
    checkPoint(after[3].points[0], 30, 0);
    checkPoint(after[3].points[1], 40, 0);

    REQUIRE(f.model->undo()); // one step takes both back
    f.settle();
    CHECK(f.elements() == before);
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.elements() == after);
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements() == before);
}

TEST_CASE("sketch trim: a line nothing crosses goes whole", "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(0, 20, 40, 20)});
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.click(20, 0.5);
    REQUIRE(f.elements().size() == 1);
    CHECK(f.elements()[0].id == "L2");
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().size() == 2);
}

TEST_CASE("sketch trim: a circle cut twice becomes an arc, uncut it goes whole",
          "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {circleOf(0, 0, 10), lineOf(0, -20, 0, 20)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.click(10, 0); // the right half
    auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    CHECK(f.count(core::SketchElementKind::circle) == 0);
    REQUIRE(f.count(core::SketchElementKind::arc) == 1);
    const core::SketchElement arc =
        *std::ranges::find(elements, core::SketchElementKind::arc, &core::SketchElement::kind);
    CHECK(arc.id == "A1");
    checkPoint(arc.points[1], 0, 10); // counter-clockwise from the top
    checkPoint(arc.points[2], 0, -10);
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements() == before);

    // a circle nothing crosses: whole
    put(f, {circleOf(50, 50, 5)});
    f.click(55, 50);
    CHECK(f.count(core::SketchElementKind::circle) == 1);
}

TEST_CASE("sketch trim: a stroke over three pieces is one undo step", "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10), lineOf(30, -10, 30, 10)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.sketch->pressAt(Drawing::down(4, 0.3), 2.0);
    f.sketch->dragTo(Drawing::down(20, 0.3), 2.0);
    f.sketch->dragTo(Drawing::down(36, 0.3), 2.0);
    f.sketch->releaseAt(Drawing::down(36, 0.3), 2.0, false, true);
    f.settle();
    CHECK(f.count(core::SketchElementKind::line) == 2); // L1 is gone, the two walls stay
    CHECK(f.elements()[0].id == "L2");
    CHECK(f.nextUndo() == "trimSketch");
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements() == before); // ONE step brings all three pieces back
}

TEST_CASE("sketch trim: a stroke that jumps over a piece takes it too", "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10), lineOf(30, -10, 30, 10)});
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.sketch->pressAt(Drawing::down(4, 0.3), 2.0);
    f.sketch->dragTo(Drawing::down(36, 0.3), 2.0); // one jump over the middle piece
    f.sketch->releaseAt(Drawing::down(36, 0.3), 2.0, false, true);
    f.settle();
    CHECK(f.count(core::SketchElementKind::line) == 2);
}

TEST_CASE("sketch trim: a press and release without moving is a click, trimming once",
          "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10), lineOf(30, -10, 30, 10)});
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.sketch->pressAt(Drawing::down(35, 0.3), 2.0);
    f.sketch->releaseAt(Drawing::down(35, 0.3), 2.0, false, false); // not dragged
    CHECK(f.elements()[0].points[1].u == Approx(40));               // nothing happened yet
    f.click(35, 0.3);                                               // the click that follows
    f.settle();
    CHECK(f.elements()[0].points[1].u == Approx(30));
    // one more step to undo, not two
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements()[0].points[1].u == Approx(40));
}

TEST_CASE("sketch trim: Esc drops a stroke, then ends the tool", "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.sketch->pressAt(Drawing::down(30, 0.3), 2.0);
    f.sketch->dragTo(Drawing::down(35, 0.3), 2.0);
    f.sketch->escape(); // the stroke
    CHECK(f.sketch->toolActive());
    f.sketch->releaseAt(Drawing::down(35, 0.3), 2.0, false, true);
    CHECK(f.elements() == before);
    f.sketch->escape(); // the tool
    CHECK_FALSE(f.sketch->toolActive());
}

TEST_CASE("sketch trim: a curve that is crossed gives a hint and stays",
          "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    core::SketchElement wave{
        "", core::SketchElementKind::spline, {{0, 0}, {10, 8}, {20, -4}, {35, 6}}, false};
    put(f, {wave, lineOf(15, -20, 15, 20)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.click(10, 8);
    CHECK(f.elements() == before);
    CHECK(f.notices.contains(QStringLiteral("curvePartial")));
    CHECK(f.nextUndo() != "trimSketch");
}

TEST_CASE("sketch trim: a curve nothing crosses goes whole", "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    core::SketchElement wave{
        "", core::SketchElementKind::spline, {{0, 0}, {10, 8}, {20, -4}, {35, 6}}, false};
    put(f, {wave, lineOf(0, 40, 40, 40)});
    f.sketch->chooseTool(QStringLiteral("trim"));
    f.click(10, 8);
    CHECK(f.count(core::SketchElementKind::spline) == 0);
    CHECK(f.notices.isEmpty());
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.count(core::SketchElementKind::spline) == 1);
}

TEST_CASE("sketch extend: a click near the end goes on to the next line, undo puts it back",
          "[ui][sketchtools][extend]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 20, 0), lineOf(50, -10, 50, 10)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("extend"));
    CHECK(f.sketch->toolName() == "extend");
    f.move(19, 0.5); // the preview is there, nothing changed yet
    CHECK(f.elements() == before);
    f.click(19, 0.5);
    auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    CHECK(elements[0].id == "L1");
    checkPoint(elements[0].points[0], 0, 0);
    checkPoint(elements[0].points[1], 50, 0);
    CHECK(f.nextUndo() == "extendSketch");

    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements() == before);
    REQUIRE(f.model->redo());
    f.settle();
    checkPoint(f.elements()[0].points[1], 50, 0);
}

TEST_CASE("sketch extend: an arc goes on along its circle", "[ui][sketchtools][extend]") {
    NoCatching free;
    Drawing f;
    const double r = 10.0;
    const double a = std::numbers::pi / 3.0;
    core::SketchElement arc{"",
                            core::SketchElementKind::arc,
                            {{0, 0}, {r, 0}, {r * std::cos(a), r * std::sin(a)}},
                            false};
    put(f, {arc, lineOf(-4, -20, -4, 20)});
    f.sketch->chooseTool(QStringLiteral("extend"));
    f.click(r * std::cos(a), r * std::sin(a));
    const auto elements = f.elements();
    REQUIRE(elements.size() == 2);
    checkPoint(elements[0].points[1], r, 0);
    checkPoint(elements[0].points[2], -4, std::sqrt(r * r - 16.0));
}

TEST_CASE("sketch extend: nothing in the way gives a hint and changes nothing",
          "[ui][sketchtools][extend]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 20, 0)});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("extend"));
    f.click(19, 0.5);
    CHECK(f.elements() == before);
    CHECK(f.notices.contains(QStringLiteral("nothingToExtend")));
    CHECK(f.nextUndo() != "extendSketch");
}

TEST_CASE("sketch trim: with the tool off nothing is trimmed", "[ui][sketchtools][trim]") {
    NoCatching free;
    Drawing f;
    put(f, {lineOf(0, 0, 40, 0), lineOf(10, -10, 10, 10)});
    const auto before = f.elements();
    f.sketch->pressAt(Drawing::down(30, 0.3), 2.0);
    f.sketch->releaseAt(Drawing::down(30, 0.3), 2.0, false, true);
    f.click(30, 0.3);
    CHECK(f.elements() == before);
}

// --- offset and corners (AP-2.1e2) ----------------------------------------------------------

namespace {

void putRectangle(Drawing& f) {
    put(f,
        {lineOf(0, 0, 40, 0), lineOf(40, 0, 40, 20), lineOf(40, 20, 0, 20), lineOf(0, 20, 0, 0)});
}

} // namespace

TEST_CASE("sketch offset: pick a chain, move to the side, click: four new lines, one undo step",
          "[ui][sketchtools][offset]") {
    NoCatching free;
    Drawing f;
    putRectangle(f);
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("offset"));
    CHECK(f.sketch->toolName() == "offset");
    CHECK_FALSE(f.sketch->drawing());
    f.click(20, 0.2); // picks the whole chain, nothing is made yet
    CHECK(f.elements() == before);
    CHECK(f.sketch->drawing());
    CHECK(f.sketch->placed() == 1);
    f.move(20, -5);
    CHECK(f.sketch->fieldCount() == 1);
    CHECK(f.sketch->readout().value("valid").toBool());
    f.click(20, -5);
    const auto after = f.elements();
    REQUIRE(after.size() == 8);
    CHECK(f.nextUndo() == "offsetSketch");
    CHECK(f.count(core::SketchElementKind::line) == 8);
    // the new rectangle is 50 x 30 round the old one
    double minU = 1e9;
    double maxU = -1e9;
    for (std::size_t i = 4; i < after.size(); ++i) {
        for (const core::SketchPoint& p : after[i].points) {
            minU = std::min(minU, p.u);
            maxU = std::max(maxU, p.u);
        }
    }
    CHECK(minU == Approx(-5.0).margin(1e-9));
    CHECK(maxU == Approx(45.0).margin(1e-9));
    CHECK_FALSE(f.sketch->drawing()); // the chain is let go

    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements() == before);
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.elements() == after); // the same ids and numbers
}

TEST_CASE("sketch offset: a typed distance replaces the pointer's, the side follows the pointer",
          "[ui][sketchtools][offset]") {
    NoCatching free;
    Drawing f;
    putRectangle(f);
    f.sketch->chooseTool(QStringLiteral("offset"));
    f.click(20, 0.2);
    f.move(20, 3); // inside
    f.sketch->setTyped(0, 7.0);
    CHECK(f.sketch->readout().value("fields").toList().first().toMap().value("typed").toBool());
    f.sketch->acceptTyped();
    const auto after = f.elements();
    REQUIRE(after.size() == 8);
    double minU = 1e9;
    for (std::size_t i = 4; i < after.size(); ++i) {
        for (const core::SketchPoint& p : after[i].points) {
            minU = std::min(minU, p.u);
        }
    }
    CHECK(minU == Approx(7.0).margin(1e-9));
}

TEST_CASE("sketch offset: the side follows the pointer however the chain runs",
          "[ui][sketchtools][offset]") {
    NoCatching free;
    Drawing f;
    // clockwise, the other way round than putRectangle
    put(f,
        {lineOf(0, 0, 0, 20), lineOf(0, 20, 40, 20), lineOf(40, 20, 40, 0), lineOf(40, 0, 0, 0)});
    f.sketch->chooseTool(QStringLiteral("offset"));
    f.click(20, 0.2);
    f.move(20, -5); // outside
    f.sketch->setTyped(0, 10.0);
    f.sketch->acceptTyped();
    const auto after = f.elements();
    REQUIRE(after.size() == 8);
    double minU = 1e9;
    double maxU = -1e9;
    for (std::size_t i = 4; i < after.size(); ++i) {
        for (const core::SketchPoint& p : after[i].points) {
            minU = std::min(minU, p.u);
            maxU = std::max(maxU, p.u);
        }
    }
    CHECK(minU == Approx(-10.0).margin(1e-9));
    CHECK(maxU == Approx(50.0).margin(1e-9));
}

TEST_CASE("sketch offset: a curve gives a hint, a distance too big for a circle makes nothing",
          "[ui][sketchtools][offset]") {
    NoCatching free;
    Drawing f;
    core::SketchElement curve{
        "", core::SketchElementKind::spline, {{0, 30}, {10, 40}, {20, 30}}, false};
    core::SketchElement ring{"", core::SketchElementKind::circle, {{0, 0}, {10, 0}}, false};
    REQUIRE(f.model->addSketchElements(f.sketch->step(), {curve}, QStringLiteral("spline")));
    put(f, {ring});
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("offset"));
    f.click(0, 30.5); // the end of the curve ... near the chord
    CHECK(f.notices.contains(QStringLiteral("curveOffset")));
    CHECK_FALSE(f.sketch->drawing());
    f.click(10, 0.1);
    CHECK(f.sketch->drawing());
    f.move(3, 0);
    f.sketch->setTyped(0, 12.0);
    f.sketch->acceptTyped();
    CHECK(f.notices.contains(QStringLiteral("offsetTooBig")));
    CHECK(f.elements() == before);
    // Esc takes the typed number back, then the chain, then the tool
    f.sketch->escape();
    CHECK(f.sketch->drawing()); // the chain is still picked
    f.sketch->escape();
    CHECK_FALSE(f.sketch->drawing());
    CHECK(f.sketch->toolActive());
    f.sketch->escape();
    CHECK_FALSE(f.sketch->toolActive());
}

TEST_CASE("sketch corner: a click rounds the corner with the last radius (2 mm at first)",
          "[ui][sketchtools][corner]") {
    NoCatching free;
    Drawing f;
    putRectangle(f);
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("fillet"));
    CHECK(f.sketch->toolName() == "fillet");
    CHECK(f.sketch->toolVariant() == 0);
    f.click(40, 0);
    const auto after = f.elements();
    REQUIRE(after.size() == 5);
    CHECK(f.nextUndo() == "roundCorner");
    CHECK(f.count(core::SketchElementKind::arc) == 1);
    const core::SketchElement& arc = after.back();
    checkPoint(arc.points[0], 38, 2);
    checkPoint(arc.points[1], 38, 0);
    checkPoint(arc.points[2], 40, 2);
    checkPoint(after[0].points[1], 38, 0); // the bottom side is shorter
    checkPoint(after[1].points[0], 40, 2);

    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements() == before);
    REQUIRE(f.model->redo());
    f.settle();
    CHECK(f.elements() == after);
}

TEST_CASE("sketch corner: pulling sets the radius, typing sets it too, and it is remembered",
          "[ui][sketchtools][corner]") {
    NoCatching free;
    Drawing f;
    putRectangle(f);
    f.sketch->chooseTool(QStringLiteral("fillet"));
    // press on the corner and pull: the middle of the arc follows the pointer
    f.sketch->pressAt(Drawing::down(40, 0), 2.0);
    f.sketch->dragTo(Drawing::down(35, 5), 2.0);
    f.sketch->releaseAt(Drawing::down(35, 5), 2.0, false, true);
    auto elements = f.elements();
    REQUIRE(elements.size() == 5);
    checkPoint(elements.back().points[0], 35, 5);
    // type a radius for the next corner
    f.move(40, 20);
    CHECK(f.sketch->fieldCount() == 1);
    f.sketch->setTyped(0, 3.0);
    f.sketch->acceptTyped();
    elements = f.elements();
    REQUIRE(elements.size() == 6);
    checkPoint(elements.back().points[0], 37, 17);
    // a click on the third corner takes the radius used last
    f.click(0, 20);
    elements = f.elements();
    REQUIRE(elements.size() == 7);
    checkPoint(elements.back().points[0], 3, 17);
}

TEST_CASE("sketch corner: too big for the short side says the biggest radius and changes nothing",
          "[ui][sketchtools][corner]") {
    NoCatching free;
    Drawing f;
    putRectangle(f);
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("fillet"));
    f.move(40, 0);
    f.sketch->setTyped(0, 25.0);
    f.sketch->acceptTyped();
    CHECK(f.notices.contains(QStringLiteral("cornerTooBig")));
    CHECK(f.sketch->noticeNumber() == Approx(19.99).margin(1e-9));
    CHECK(f.elements() == before);
}

TEST_CASE("sketch corner: chamfer is the second way of the tool and has its own name",
          "[ui][sketchtools][corner]") {
    NoCatching free;
    Drawing f;
    putRectangle(f);
    f.sketch->chooseTool(QStringLiteral("chamfer"));
    CHECK(f.sketch->toolName() == "fillet");
    CHECK(f.sketch->toolVariant() == 1);
    f.move(40, 20);
    f.sketch->setTyped(0, 4.0);
    f.sketch->acceptTyped();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 5);
    CHECK(f.nextUndo() == "chamferCorner");
    CHECK(elements.back().kind == core::SketchElementKind::line);
    CHECK(f.count(core::SketchElementKind::arc) == 0);
    // the ways are remembered: choosing the tool again chamfers
    f.sketch->chooseTool(QStringLiteral("fillet"));
    CHECK(f.sketch->toolVariant() == 1);
    f.sketch->chooseTool(QStringLiteral("fillet"), 0);
    CHECK(f.sketch->toolVariant() == 0);
}

TEST_CASE("sketch corner: a curve and a click far from any corner give hints",
          "[ui][sketchtools][corner]") {
    NoCatching free;
    Drawing f;
    core::SketchElement curve{
        "", core::SketchElementKind::spline, {{10, 0}, {15, 5}, {20, 0}}, false};
    put(f, {lineOf(0, 0, 10, 0)});
    REQUIRE(f.model->addSketchElements(f.sketch->step(), {curve}, QStringLiteral("spline")));
    f.settle();
    const auto before = f.elements();
    f.sketch->chooseTool(QStringLiteral("fillet"));
    f.click(10, 0);
    CHECK(f.notices.contains(QStringLiteral("curveCorner")));
    f.click(2, 0.1);
    CHECK(f.notices.contains(QStringLiteral("noCorner")));
    CHECK(f.elements() == before);
}
