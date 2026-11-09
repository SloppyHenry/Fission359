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
