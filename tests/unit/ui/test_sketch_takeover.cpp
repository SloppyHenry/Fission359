// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The tool "take over edge" of the sketch mode (AP-2.1f) through the controller: an edge becomes a
// linked element, a face gives its whole border, nothing twice, only bodies from before the sketch,
// the elements follow the body, protected elements say why and let go of the link with one click,
// Undo and Redo bring the same ids back. No window: the pick pass is replaced by the calls its
// answer leads to.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <memory>

#include "fission/core/selection.h"
#include "fission/geo/occt/kernel.h"
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

struct NoCatching {
    NoCatching() { fission::ui::setSnappingSuppressedForTests(true); }
    ~NoCatching() { fission::ui::setSnappingSuppressedForTests(false); }
};

struct Taking {
    Taking() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        selection = std::make_unique<SelectionController>(model.get());
        sketch = std::make_unique<SketchController>(model.get(), selection.get());
        QObject::connect(sketch.get(), &SketchController::notice, &holder,
                         [this](const QString& code) { notices << code; });
    }

    void settle() {
        model->waitForCalculation();
        QCoreApplication::processEvents();
        model->waitForCalculation(); // the followed points are written and calculated once more
        QCoreApplication::processEvents();
    }

    int faceNamed(const char* name) const {
        const auto result = model->result();
        REQUIRE(result != nullptr);
        for (const auto& [id, made] : result->bodies) {
            for (const core::BodyFace& face : made.faces) {
                if (face.name == name) {
                    return static_cast<int>(face.faceId);
                }
            }
        }
        FAIL("no face named " << name);
        return 0;
    }

    // A box 40 x 20 x 10 and a sketch on its top with the tool "take over edge" on.
    void boxWithSketch() {
        REQUIRE(model->addShape("box", false) >= 0);
        model->setStepValue(0, QStringLiteral("width"), QStringLiteral("40"));
        model->setStepValue(0, QStringLiteral("depth"), QStringLiteral("20"));
        model->setStepValue(0, QStringLiteral("height"), QStringLiteral("10"));
        settle();
        body = model->bodyIdAt(0);
        sketch->start();
        REQUIRE(sketch->pickFace(body, faceNamed("F1/box/top")));
        settle();
        REQUIRE(sketch->planeReady());
        sketch->chooseTool(QStringLiteral("takeover"));
        REQUIRE(sketch->toolActive());
    }

    std::vector<core::SketchElement> elements() const {
        return model->sketchElements(sketch->step());
    }
    std::size_t linked() const {
        std::size_t n = 0;
        for (const core::SketchElement& element : elements()) {
            n += element.link ? 1 : 0;
        }
        return n;
    }
    QString nextUndo() const { return model->nextUndo().value("key").toString(); }

    // The width of the box round the points of all elements.
    std::pair<double, double> extent() const {
        double low[2] = {1e300, 1e300};
        double high[2] = {-1e300, -1e300};
        for (const core::SketchElement& element : elements()) {
            for (const core::SketchPoint& point : element.points) {
                low[0] = std::min(low[0], point.u);
                low[1] = std::min(low[1], point.v);
                high[0] = std::max(high[0], point.u);
                high[1] = std::max(high[1], point.v);
            }
        }
        return {high[0] - low[0], high[1] - low[1]};
    }

    std::shared_ptr<fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    std::unique_ptr<SelectionController> selection;
    std::unique_ptr<SketchController> sketch;
    QObject holder;
    QStringList notices;
    QString body;
};

constexpr int kEdge = static_cast<int>(core::SelectableKind::edge);
constexpr int kFace = static_cast<int>(core::SelectableKind::face);

} // namespace

TEST_CASE("take over: the tool exists only inside a sketch and lets the pointer find edges",
          "[ui][takeover]") {
    Taking f;
    f.sketch->chooseTool(QStringLiteral("takeover"));
    CHECK_FALSE(f.sketch->toolActive());
    f.boxWithSketch();
    CHECK(f.sketch->toolName() == "takeover");
    // the tool does not draw: a click on the plane does nothing
    f.sketch->clickAt({{0.0, 0.0, 100.0}, {0.0, 0.0, -1.0}}, 2.0);
    CHECK(f.elements().empty());
    f.sketch->escape(); // the tool first
    CHECK_FALSE(f.sketch->toolActive());
    CHECK(f.sketch->active());
}

TEST_CASE("take over: a click on a face away from its edges takes the whole border in one step",
          "[ui][takeover]") {
    NoCatching free;
    Taking f;
    f.boxWithSketch();
    const auto steps = f.model->recipe().size();
    REQUIRE(f.sketch->takeOver(f.body, kFace, f.faceNamed("F1/box/top")));
    f.settle();
    const auto elements = f.elements();
    REQUIRE(elements.size() == 4);
    for (const core::SketchElement& element : elements) {
        CHECK(element.kind == core::SketchElementKind::line);
        REQUIRE(element.link.has_value());
        CHECK(element.link->input == 0);
        CHECK_FALSE(element.construction);
    }
    // The border of the top is the rectangle 40 x 20, on the plane of the face
    CHECK(f.extent().first == Approx(40.0).margin(1e-9));
    CHECK(f.extent().second == Approx(20.0).margin(1e-9));
    CHECK(f.model->recipe().size() == steps); // no new step: the sketch step has the elements
    CHECK(f.nextUndo() == "takeOverEdges");
    // The recipe sentence has its count
    const auto next = f.model->nextUndo();
    CHECK(next.value("count").toString() == "4");

    // Undo is one step and takes all four away, with their reference and numbers; redo gives the
    // same ids back
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.elements().empty());
    REQUIRE(f.model->redo());
    f.settle();
    const auto again = f.elements();
    REQUIRE(again.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(again[i].id == elements[i].id);
        CHECK(again[i].link == elements[i].link);
    }
}

TEST_CASE("take over: nothing is taken twice", "[ui][takeover]") {
    NoCatching free;
    Taking f;
    f.boxWithSketch();
    REQUIRE(f.sketch->takeOver(f.body, kFace, f.faceNamed("F1/box/top")));
    f.settle();
    f.notices.clear();
    CHECK_FALSE(f.sketch->takeOver(f.body, kFace, f.faceNamed("F1/box/top")));
    CHECK(f.notices == QStringList{"edgeTaken"});
    CHECK(f.elements().size() == 4);
    // a single edge of the border is taken already, too; an edge of the side is new
    f.notices.clear();
    CHECK_FALSE(f.sketch->takeOverNamed(QStringLiteral("edge[F1/box/right|F1/box/top]")));
    CHECK(f.notices == QStringList{"edgeTaken"});
    CHECK(f.sketch->takeOverNamed(QStringLiteral("edge[F1/box/front|F1/box/right]")) == false);
    CHECK(f.notices.back() == "cannotTake"); // runs along the normal: it would be a point
    CHECK(f.elements().size() == 4);
}

TEST_CASE("take over: the line follows the box when it is made wider", "[ui][takeover]") {
    NoCatching free;
    Taking f;
    f.boxWithSketch();
    REQUIRE(f.sketch->takeOverNamed(QStringLiteral("edge[F1/box/right|F1/box/top]")));
    f.settle();
    REQUIRE(f.elements().size() == 1);
    const auto before = f.elements().front().points;
    CHECK(std::hypot(before[1].u - before[0].u, before[1].v - before[0].v) ==
          Approx(20.0).margin(1e-9));

    REQUIRE(f.model->setStepValue(0, QStringLiteral("width"), QStringLiteral("60")));
    f.settle();
    const auto after = f.elements().front().points; // the document holds the followed points
    // the edge was 20 long and stays 20 long, but moved out by 10 mm (the plane's middle stays in
    // the face's middle)
    CHECK(std::hypot(after[1].u - after[0].u, after[1].v - after[0].v) ==
          Approx(20.0).margin(1e-9));
    const double moved = std::hypot(after[0].u - before[0].u, after[0].v - before[0].v);
    CHECK(moved == Approx(10.0).margin(1e-9));
    // and the whole border follows exactly
    REQUIRE(f.sketch->takeOver(f.body, kFace, f.faceNamed("F1/box/top")));
    f.settle();
    CHECK(f.extent().first == Approx(60.0).margin(1e-9));
}

TEST_CASE("take over: only bodies that come before the sketch can be taken from",
          "[ui][takeover]") {
    NoCatching free;
    Taking f;
    f.boxWithSketch();
    REQUIRE(f.model->addShape("cylinder", false) >= 0); // a step after the sketch
    f.settle();
    const QString later = f.model->bodyIdAt(1);
    REQUIRE(!later.isEmpty());
    const QStringList takeable = f.model->takeableBodies(f.sketch->step());
    CHECK(takeable.contains(f.body));
    CHECK_FALSE(takeable.contains(later));
    f.notices.clear();
    CHECK_FALSE(f.sketch->takeOver(later, kEdge, 1));
    CHECK(f.notices == QStringList{"cannotTake"});
    CHECK(f.elements().empty());
}

TEST_CASE("take over: a circle is taken over, a tilted one is not (it would be an ellipse)",
          "[ui][takeover]") {
    NoCatching free;
    Taking f;
    REQUIRE(f.model->addShape("cylinder", false) >= 0);
    f.settle();
    f.body = f.model->bodyIdAt(0);
    f.sketch->start();
    REQUIRE(f.sketch->pickFace(f.body, f.faceNamed("F1/cylinder/top")));
    f.settle();
    f.sketch->chooseTool(QStringLiteral("takeover"));
    REQUIRE(f.sketch->takeOverNamed(QStringLiteral("edge[F1/cylinder/side|F1/cylinder/top]")));
    f.settle();
    REQUIRE(f.elements().size() == 1);
    CHECK(f.elements().front().kind == core::SketchElementKind::circle);
    CHECK(f.elements().front().link.has_value());
    f.sketch->finish();
    f.settle();

    // A sketch on the ground and a cylinder tilted about X: its top circle is an ellipse there
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotX"), QStringLiteral("30")));
    f.settle();
    f.sketch->start();
    REQUIRE(f.sketch->pickGround());
    f.settle();
    f.sketch->chooseTool(QStringLiteral("takeover"));
    f.notices.clear();
    CHECK_FALSE(f.sketch->takeOverNamed(QStringLiteral("edge[F1/cylinder/side|F1/cylinder/top]")));
    CHECK(f.notices.back() == "cannotTake");
}

TEST_CASE("take over: protected elements say so, and one click lets go of the link",
          "[ui][takeover]") {
    NoCatching free;
    Taking f;
    f.boxWithSketch();
    REQUIRE(f.sketch->takeOver(f.body, kFace, f.faceNamed("F1/box/top")));
    f.settle();
    REQUIRE(f.linked() == 4);

    // Rounding a corner of the border changes two linked lines: refused, with the offer
    f.sketch->chooseTool(QStringLiteral("fillet"));
    f.notices.clear();
    const auto plane = f.model->sketchResult(f.sketch->step())->plane;
    REQUIRE(plane.has_value());
    const auto elements = f.elements();
    // the corner: the end of the first line that another line ends in, too
    core::SketchPoint corner = elements[0].points[0];
    const auto world = plane->toWorld(corner);
    const interaction::PlaneRay ray{{world.x, world.y, world.z + 100.0}, {0.0, 0.0, -1.0}};
    f.sketch->pointAt(ray, 2.0);
    f.sketch->clickAt(ray, 2.0);
    CHECK(f.notices.contains("linked"));
    CHECK(f.sketch->linkedBlocked());
    CHECK(f.elements() == elements); // nothing changed
    CHECK(f.linked() == 4);

    // One click lets go of the two lines that were touched (one step of the history)
    REQUIRE(f.sketch->releaseLinks());
    CHECK_FALSE(f.sketch->linkedBlocked());
    CHECK(f.linked() == 2);
    CHECK(f.nextUndo() == "unlinkSketchElement");
    // Now rounding the corner works, as for any element
    f.sketch->clickAt(ray, 2.0);
    CHECK(f.elements().size() == 5);
    // The released lines keep their points; the others still follow
    CHECK(f.linked() == 2);
    // Undo the rounding and the letting go: linked again, same ids
    REQUIRE(f.model->undo());
    REQUIRE(f.model->undo());
    f.settle();
    CHECK(f.linked() == 4);
    CHECK(f.elements() == elements);
}

TEST_CASE("take over: copies of linked elements are plain elements", "[ui][takeover]") {
    NoCatching free;
    Taking f;
    f.boxWithSketch();
    REQUIRE(f.sketch->takeOverNamed(QStringLiteral("edge[F1/box/right|F1/box/top]")));
    f.settle();
    REQUIRE(f.linked() == 1);
    // Offsetting the line makes a new element without a link; the original stays linked
    f.sketch->chooseTool(QStringLiteral("offset"));
    const auto plane = f.model->sketchResult(f.sketch->step())->plane;
    const auto line = f.elements().front();
    const core::SketchPoint middle{(line.points[0].u + line.points[1].u) / 2.0,
                                   (line.points[0].v + line.points[1].v) / 2.0};
    const auto at = [&](double du) {
        const auto w = plane->toWorld({middle.u + du, middle.v});
        return interaction::PlaneRay{{w.x, w.y, w.z + 100.0}, {0.0, 0.0, -1.0}};
    };
    f.sketch->pointAt(at(0.0), 2.0);
    f.sketch->clickAt(at(0.0), 2.0);
    f.sketch->pointAt(at(-5.0), 2.0);
    f.sketch->setTyped(0, 5.0);
    f.sketch->acceptTyped();
    f.settle();
    REQUIRE(f.elements().size() == 2);
    CHECK(f.linked() == 1);
    CHECK(f.elements().front().link.has_value());
    CHECK_FALSE(f.elements().back().link.has_value());
}
