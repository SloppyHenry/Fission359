// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The drawing tools of the sketch mode (AP-2.1c1) as a state machine: line chain, rectangle,
// circle, point; typed numbers; helper lines; elements that are too small.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "fission/interaction/sketch_tool.h"

using Catch::Approx;
using namespace fission::interaction;
using fission::core::SketchElement;
using fission::core::SketchElementKind;
using fission::core::SketchPoint;

namespace {

void checkPoint(const SketchPoint& point, double u, double v) {
    CHECK(point.u == Approx(u).margin(1e-9));
    CHECK(point.v == Approx(v).margin(1e-9));
}

// The batches of a step, flattened.
std::vector<SketchElement> all(const SketchToolStep& step) {
    std::vector<SketchElement> out;
    for (const SketchBatch& batch : step.done) {
        out.insert(out.end(), batch.begin(), batch.end());
    }
    return out;
}

} // namespace

TEST_CASE("sketch tool: no tool draws nothing", "[sketchtool]") {
    SketchTool tool;
    CHECK(tool.kind() == SketchToolKind::none);
    CHECK(tool.click({1, 1}).done.empty());
    CHECK_FALSE(tool.busy());
}

TEST_CASE("sketch tool: a line chain makes one line per click after the first, ends share points",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    CHECK(tool.click({0, 0}).done.empty());
    CHECK(tool.busy());
    const auto first = tool.click({10, 0});
    REQUIRE(first.done.size() == 1);
    REQUIRE(first.done[0].size() == 1);
    CHECK(first.done[0][0].kind == SketchElementKind::line);
    checkPoint(first.done[0][0].points[0], 0, 0);
    checkPoint(first.done[0][0].points[1], 10, 0);
    const auto second = tool.click({10, 7});
    REQUIRE(second.done.size() == 1);
    // the chain: the second line starts exactly where the first ended
    CHECK(second.done[0][0].points[0] == first.done[0][0].points[1]);
    checkPoint(second.done[0][0].points[1], 10, 7);
    CHECK(tool.busy());
}

TEST_CASE(
    "sketch tool: a double click ends the chain, Esc too, and the next click starts a new one",
    "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.click({10, 0});
    // a double click is a click (the point is made) and then the double click event
    CHECK(tool.click({10, 10}).done.size() == 1);
    CHECK(tool.doubleClick().done.empty());
    CHECK_FALSE(tool.busy());
    CHECK(tool.click({20, 20}).done.empty()); // starts a new chain
    CHECK(tool.busy());
    CHECK(tool.cancel());
    CHECK_FALSE(tool.busy());
    CHECK_FALSE(tool.cancel()); // nothing was running
}

TEST_CASE("sketch tool: a line shorter than 0.01 mm is not drawn and the chain waits",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    const auto step = tool.click({0.004, 0});
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
    CHECK(tool.busy());
    CHECK(tool.click({5, 0}).done.size() == 1); // the first point is still the start
}

TEST_CASE("sketch tool: the preview of a line follows the pointer", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    CHECK(tool.draft().elements.empty());
    tool.click({1, 2});
    tool.moveTo({4, 6});
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.elements.size() == 1);
    checkPoint(draft.elements[0].points[1], 4, 6);
    REQUIRE(draft.fields.size() == 2);
    CHECK(draft.fields[0].name == "length");
    CHECK(draft.fields[0].value == Approx(5.0));
    CHECK_FALSE(draft.fields[0].typed);
    CHECK(draft.fields[1].name == "angle");
}

TEST_CASE("sketch tool: typing a length and an angle sets the line", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({10, 10});
    tool.moveTo({20, 10}); // the pointer points along +u
    CHECK(tool.fieldCount() == 2);
    tool.setTyped(0, 30.0);
    SketchDraft draft = tool.draft();
    checkPoint(draft.elements[0].points[1], 40, 10); // direction from the pointer, length typed
    CHECK(draft.fields[0].typed);
    tool.setTyped(1, 90.0);
    draft = tool.draft();
    checkPoint(draft.elements[0].points[1], 10, 40);
    CHECK(draft.fields[1].typed);
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[0], 10, 10);
    checkPoint(step.done[0][0].points[1], 10, 40);
    CHECK_FALSE(tool.hasTyped()); // the next segment starts clean
    CHECK(tool.busy());           // the chain goes on from the end
}

TEST_CASE("sketch tool: a negative length turns the line round", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.moveTo({5, 0});
    tool.setTyped(0, -8.0);
    checkPoint(tool.draft().elements[0].points[1], -8, 0);
}

TEST_CASE("sketch tool: typing a length of nothing draws nothing", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.moveTo({5, 0});
    tool.setTyped(0, 0.0);
    const auto step = tool.accept();
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
}

TEST_CASE("sketch tool: a rectangle by two corners is four lines in one batch", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle);
    CHECK(tool.click({10, 5}).done.empty());
    const auto step = tool.click({40, 25});
    REQUIRE(step.done.size() == 1); // one batch: one undo step
    const SketchBatch& lines = step.done[0];
    REQUIRE(lines.size() == 4);
    for (const SketchElement& line : lines) {
        CHECK(line.kind == SketchElementKind::line);
    }
    // a closed loop, counter-clockwise from the lower left
    checkPoint(lines[0].points[0], 10, 5);
    checkPoint(lines[0].points[1], 40, 5);
    checkPoint(lines[1].points[1], 40, 25);
    checkPoint(lines[2].points[1], 10, 25);
    checkPoint(lines[3].points[1], 10, 5);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(lines[i].points[1] == lines[(i + 1) % 4].points[0]);
    }
    CHECK_FALSE(tool.busy()); // the tool stays, ready for the next rectangle
    CHECK(tool.kind() == SketchToolKind::rectangle);
}

TEST_CASE("sketch tool: the rectangle does not depend on the direction of the drag",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle);
    tool.click({40, 25});
    const auto step = tool.click({10, 5});
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[0], 10, 5);
    checkPoint(step.done[0][2].points[0], 40, 25);
}

TEST_CASE("sketch tool: a rectangle by its middle and a corner", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle, kSketchVariantSecond);
    tool.click({0, 0});
    const auto step = tool.click({15, 10});
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 4);
    checkPoint(step.done[0][0].points[0], -15, -10);
    checkPoint(step.done[0][2].points[0], 15, 10);
}

TEST_CASE("sketch tool: typing a width and a height sets the rectangle", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle);
    tool.click({0, 0});
    tool.moveTo({3, 3}); // the pointer is up and to the right: the sizes go that way
    tool.setTyped(0, 50.0);
    tool.setTyped(1, 20.0);
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 2);
    CHECK(draft.fields[0].value == Approx(50.0));
    CHECK(draft.fields[1].value == Approx(20.0));
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[0], 0, 0);
    checkPoint(step.done[0][2].points[0], 50, 20);
}

TEST_CASE("sketch tool: typed sizes of a rectangle go to the side the pointer is on",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle);
    tool.click({0, 0});
    tool.moveTo({-3, -3});
    tool.setTyped(0, 50.0);
    tool.setTyped(1, 20.0);
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[0], -50, -20);
    checkPoint(step.done[0][2].points[0], 0, 0);
}

TEST_CASE("sketch tool: typed width and height of a rectangle by its middle are whole sizes",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle, kSketchVariantSecond);
    tool.click({10, 10});
    tool.moveTo({11, 11});
    tool.setTyped(0, 40.0);
    tool.setTyped(1, 20.0);
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[0], -10, 0);
    checkPoint(step.done[0][2].points[0], 30, 20);
}

TEST_CASE("sketch tool: a rectangle with a side under 0.01 mm is not drawn", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::rectangle);
    tool.click({0, 0});
    const auto step = tool.click({30, 0.005});
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
    CHECK(tool.busy());
}

TEST_CASE("sketch tool: a circle by its middle and a point on it", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::circle);
    CHECK(tool.click({5, 5}).done.empty());
    const auto step = tool.click({5, 15});
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 1);
    const SketchElement& circle = step.done[0][0];
    CHECK(circle.kind == SketchElementKind::circle);
    checkPoint(circle.points[0], 5, 5);
    checkPoint(circle.points[1], 5, 15);
    CHECK_FALSE(tool.busy());
}

TEST_CASE("sketch tool: a typed radius sets the circle, in the direction of the pointer",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::circle);
    tool.click({0, 0});
    tool.moveTo({0, 3});
    CHECK(tool.fieldCount() == 1);
    tool.setTyped(0, 12.5);
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 1);
    CHECK(draft.fields[0].name == "radius");
    CHECK(draft.fields[0].value == Approx(12.5));
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[1], 0, 12.5);
}

TEST_CASE("sketch tool: a circle with a radius under 0.01 mm is not drawn", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::circle);
    tool.click({0, 0});
    const auto step = tool.click({0.001, 0});
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
}

TEST_CASE("sketch tool: a circle through three points is turned into middle and edge point",
          "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::circle, kSketchVariantSecond);
    CHECK(tool.click({10, 0}).done.empty());
    CHECK(tool.click({0, 10}).done.empty());
    CHECK(tool.fieldCount() == 0); // nothing to type for three points
    const auto step = tool.click({-10, 0});
    REQUIRE(step.done.size() == 1);
    const SketchElement& circle = step.done[0][0];
    CHECK(circle.kind == SketchElementKind::circle);
    checkPoint(circle.points[0], 0, 0);
    checkPoint(circle.points[1], 10, 0); // the first point is on the circle
    CHECK_FALSE(tool.busy());
}

TEST_CASE("sketch tool: three points on one line make no circle, with a notice", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::circle, kSketchVariantSecond);
    tool.click({0, 0});
    tool.click({10, 10});
    const auto step = tool.click({20, 20});
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::collinear);
    CHECK(tool.placedCount() == 2); // another third point still works
    CHECK(tool.click({0, 20}).done.size() == 1);
}

TEST_CASE("sketch tool: three points of which two are the same make no circle", "[sketchtool]") {
    CHECK_FALSE(circleThroughThreePoints({1, 1}, {1, 1}, {5, 5}).has_value());
    CHECK_FALSE(circleThroughThreePoints({0, 0}, {5, 0}, {10, 0}).has_value());
    const auto circle = circleThroughThreePoints({0, 0}, {4, 0}, {0, 3});
    REQUIRE(circle.has_value());
    checkPoint(circle->points[0], 2, 1.5);
    CHECK(std::hypot(circle->points[1].u - 2, circle->points[1].v - 1.5) == Approx(2.5));
}

TEST_CASE("sketch tool: the preview of a circle through three points", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::circle, kSketchVariantSecond);
    tool.click({10, 0});
    tool.moveTo({0, 10});
    SketchDraft draft = tool.draft();
    REQUIRE_FALSE(draft.elements.empty()); // a guide line and the point
    tool.click({0, 10});
    tool.moveTo({-10, 0});
    draft = tool.draft();
    bool circle = false;
    for (const SketchElement& element : draft.elements) {
        circle = circle || element.kind == SketchElementKind::circle;
    }
    CHECK(circle);
}

TEST_CASE("sketch tool: the point tool places a point at each click", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::point);
    const auto one = tool.click({3, 4});
    REQUIRE(one.done.size() == 1);
    REQUIRE(one.done[0].size() == 1);
    CHECK(one.done[0][0].kind == SketchElementKind::point);
    checkPoint(one.done[0][0].points[0], 3, 4);
    CHECK(tool.click({5, 6}).done.size() == 1);
    CHECK_FALSE(tool.busy());
    CHECK(tool.fieldCount() == 0);
}

TEST_CASE("sketch tool: the helper line switch and the Alt key make aids", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    auto step = tool.click({10, 0});
    CHECK_FALSE(all(step)[0].construction);

    tool.setHelperLine(true);
    step = tool.click({10, 10});
    CHECK(all(step)[0].construction);
    tool.setHelperLine(false);

    step = tool.click({20, 10}, /*alt=*/true); // Alt held at the click
    CHECK(all(step)[0].construction);
    step = tool.click({30, 10}, false);
    CHECK_FALSE(all(step)[0].construction);

    // all four sides of a rectangle, the circle and the point
    tool.select(SketchToolKind::rectangle);
    tool.click({0, 0}, true);
    step = tool.click({5, 5}, true);
    for (const SketchElement& line : all(step)) {
        CHECK(line.construction);
    }
    tool.select(SketchToolKind::circle);
    tool.setHelperLine(true);
    tool.click({0, 0});
    CHECK(all(tool.click({0, 5}))[0].construction);
    tool.select(SketchToolKind::point);
    CHECK(all(tool.click({1, 1}))[0].construction);
}

TEST_CASE("sketch tool: the preview shows the helper line dashed state too", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.moveTo({5, 5}, /*alt=*/true);
    CHECK(tool.draft().elements[0].construction);
    tool.moveTo({5, 5}, false);
    CHECK_FALSE(tool.draft().elements[0].construction);
}

TEST_CASE("sketch tool: choosing a tool drops the running element", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.setTyped(0, 5.0);
    tool.select(SketchToolKind::rectangle);
    CHECK_FALSE(tool.busy());
    CHECK_FALSE(tool.hasTyped());
    tool.select(SketchToolKind::none);
    CHECK(tool.click({1, 1}).done.empty());
}

TEST_CASE("sketch tool: accepting with nothing running does nothing", "[sketchtool]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    CHECK(tool.accept().done.empty());
    tool.select(SketchToolKind::circle, kSketchVariantSecond);
    tool.click({0, 0});
    CHECK(tool.accept().done.empty()); // three points have nothing to type
}

// --- AP-2.1c2: arc, tangential arc, slot, polygon ---------------------------------------------

TEST_CASE("sketch tool: an arc by three points, counter-clockwise as it is", "[sketchtool][arc]") {
    SketchTool tool;
    tool.select(SketchToolKind::arc);
    CHECK(tool.click({10, 0}).done.empty());
    CHECK(tool.fieldCount() == 0); // nothing to type before the end is set
    CHECK(tool.click({-10, 0}).done.empty());
    CHECK(tool.fieldCount() == 1); // the radius
    tool.moveTo({0, 10});
    const SketchDraft draft = tool.draft();
    REQUIRE_FALSE(draft.fields.empty());
    CHECK(draft.fields[0].name == "radius");
    CHECK(draft.fields[0].value == Approx(10.0));
    const auto step = tool.click({0, 10});
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 1);
    const SketchElement& arc = step.done[0][0];
    CHECK(arc.kind == SketchElementKind::arc);
    checkPoint(arc.points[0], 0, 0);
    checkPoint(arc.points[1], 10, 0);
    checkPoint(arc.points[2], -10, 0);
    CHECK_FALSE(tool.busy());
}

TEST_CASE("sketch tool: drawn clockwise the arc has start and end swapped", "[sketchtool][arc]") {
    SketchTool tool;
    tool.select(SketchToolKind::arc);
    tool.click({-10, 0});
    tool.click({10, 0});
    const auto step = tool.click({0, 10}); // over the top from the left to the right: clockwise
    REQUIRE(step.done.size() == 1);
    const SketchElement& arc = step.done[0][0];
    checkPoint(arc.points[0], 0, 0);
    checkPoint(arc.points[1], 10, 0);
    checkPoint(arc.points[2], -10, 0);
}

TEST_CASE("sketch tool: three points on a line make no arc, another third point goes on",
          "[sketchtool][arc]") {
    SketchTool tool;
    tool.select(SketchToolKind::arc);
    tool.click({0, 0});
    tool.click({10, 0});
    const auto step = tool.click({5, 0});
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::collinear);
    CHECK(tool.placedCount() == 2);
    CHECK(tool.click({5, 4}).done.size() == 1);
}

TEST_CASE("sketch tool: an arc with a typed radius smaller than half the chord is not drawn",
          "[sketchtool][arc]") {
    SketchTool tool;
    tool.select(SketchToolKind::arc);
    tool.click({0, 0});
    tool.click({10, 0});
    tool.moveTo({5, 3});
    tool.setTyped(0, 4.9);
    CHECK(tool.draft().fields[0].typed);
    const auto step = tool.accept();
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
    CHECK(tool.placedCount() == 2);

    tool.setTyped(0, 13.0); // the pointer is above the chord: the arc bulges up
    const auto done = tool.accept();
    REQUIRE(done.done.size() == 1);
    const SketchElement& arc = done.done[0][0];
    checkPoint(arc.points[0], 5, -12);
    // walked from (0,0) to (10,0) with the bulge on the left: clockwise, so swapped
    checkPoint(arc.points[1], 10, 0);
    checkPoint(arc.points[2], 0, 0);
}

TEST_CASE("sketch tool: an arc by middle, start and end follows the pointer round",
          "[sketchtool][arc]") {
    SketchTool tool;
    tool.select(SketchToolKind::arc, kSketchVariantSecond);
    tool.click({0, 0});
    tool.click({10, 0});
    CHECK(tool.fieldCount() == 1);
    // turn the pointer a quarter, then on to 270 degrees: more than a half circle
    tool.moveTo({7, 7});
    tool.moveTo({0, 10});
    tool.moveTo({-10, 0});
    tool.moveTo({0, -10});
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 1);
    CHECK(draft.fields[0].name == "angle");
    CHECK(draft.fields[0].value == Approx(270.0));
    const auto step = tool.click({0, -10});
    REQUIRE(step.done.size() == 1);
    const SketchElement& arc = step.done[0][0];
    checkPoint(arc.points[0], 0, 0);
    checkPoint(arc.points[1], 10, 0);
    checkPoint(arc.points[2], 0, -10);
}

TEST_CASE("sketch tool: an arc by middle, start and end drawn clockwise swaps, typed angle works",
          "[sketchtool][arc]") {
    SketchTool tool;
    tool.select(SketchToolKind::arc, kSketchVariantSecond);
    tool.click({0, 0});
    tool.click({10, 0});
    tool.moveTo({7, -7});
    tool.moveTo({0, -10});
    const auto cw = tool.click({0, -10});
    REQUIRE(cw.done.size() == 1);
    checkPoint(cw.done[0][0].points[1], 0, -10);
    checkPoint(cw.done[0][0].points[2], 10, 0);

    tool.click({0, 0});
    tool.click({0, 10});
    tool.moveTo({1, 10});
    tool.setTyped(0, 90.0); // positive: counter-clockwise
    const auto typed = tool.accept();
    REQUIRE(typed.done.size() == 1);
    checkPoint(typed.done[0][0].points[1], 0, 10);
    checkPoint(typed.done[0][0].points[2], -10, 0);

    tool.click({0, 0});
    tool.click({10, 0});
    tool.setTyped(0, 360.0); // a full circle is no arc
    const auto full = tool.accept();
    CHECK(full.done.empty());
    CHECK(full.notice == SketchToolNotice::tooSmall);
}

TEST_CASE("sketch tool: pulling on the end of a line makes an arc that joins it",
          "[sketchtool][tangent]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    CHECK_FALSE(tool.press({0, 0}, 5, 3)); // nothing drawn yet
    tool.click({0, 0});
    CHECK_FALSE(tool.press({0, 0}, 5, 3)); // no line in the chain yet
    tool.click({20, 0});
    CHECK_FALSE(tool.press({10, 0}, 5, 3)); // not on the end
    REQUIRE(tool.press({20, 0}, 5, 3));
    CHECK_FALSE(tool.dragging());
    tool.moveTo({21, 1}); // under the limit
    CHECK_FALSE(tool.dragging());
    tool.moveTo({30, 10});
    REQUIRE(tool.dragging());
    CHECK(tool.fieldCount() == 1);
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 1);
    CHECK(draft.fields[0].name == "radius");
    CHECK(draft.fields[0].value == Approx(10.0));
    REQUIRE(draft.elements.size() == 1);
    CHECK(draft.elements[0].kind == SketchElementKind::arc);

    const auto step = tool.release({30, 10});
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 1);
    const SketchElement& arc = step.done[0][0];
    CHECK(arc.kind == SketchElementKind::arc);
    checkPoint(arc.points[0], 20, 10);
    checkPoint(arc.points[1], 20, 0);
    checkPoint(arc.points[2], 30, 10);
    // the tangent at the start is the direction of the line (1, 0), on 1e-9
    const double ru = arc.points[1].u - arc.points[0].u;
    const double rv = arc.points[1].v - arc.points[0].v;
    const double r = std::hypot(ru, rv);
    CHECK(-rv / r == Approx(1.0).margin(1e-9));
    CHECK(ru / r == Approx(0.0).margin(1e-9));
    CHECK_FALSE(tool.dragging());

    // the chain goes on from the end of the arc, as a line, in the direction the arc ended
    CHECK(tool.busy());
    const auto next = tool.click({30, 30});
    REQUIRE(next.done.size() == 1);
    CHECK(next.done[0][0].kind == SketchElementKind::line);
    checkPoint(next.done[0][0].points[0], 30, 10);
    // and another arc can join that line
    REQUIRE(tool.press({30, 30}, 5, 3));
    tool.moveTo({20, 40});
    const auto second = tool.release({20, 40});
    REQUIRE(second.done.size() == 1);
    CHECK(second.done[0][0].kind == SketchElementKind::arc);
}

TEST_CASE("sketch tool: an arc after an arc keeps the direction of the end of that arc",
          "[sketchtool][tangent]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.click({20, 0});
    REQUIRE(tool.press({20, 0}, 5, 3));
    tool.moveTo({30, -10}); // to the right of the line: clockwise
    const auto first = tool.release({30, -10});
    REQUIRE(first.done.size() == 1);
    const SketchElement& arc = first.done[0][0];
    // clockwise: the element runs from the end to the start
    checkPoint(arc.points[0], 20, -10);
    checkPoint(arc.points[1], 30, -10);
    checkPoint(arc.points[2], 20, 0);
    // the walk ends going down: a line to a point straight below goes on in that direction
    REQUIRE(tool.press({30, -10}, 5, 3));
    tool.moveTo({30, -30});
    tool.moveTo({30, -30});
    const auto straight = tool.release({30, -30});
    CHECK(straight.done.empty());
    CHECK(straight.notice == SketchToolNotice::collinear); // straight on: no arc
}

TEST_CASE("sketch tool: Esc while an arc is pulled drops only the arc; a click without pulling is "
          "a click",
          "[sketchtool][tangent]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.click({20, 0});
    REQUIRE(tool.press({20, 0}, 5, 3));
    tool.moveTo({30, 10});
    REQUIRE(tool.dragging());
    CHECK(tool.cancel());
    CHECK_FALSE(tool.dragging());
    CHECK(tool.busy()); // the chain stays
    CHECK(tool.release({30, 10}).done.empty());

    REQUIRE(tool.press({20, 0}, 5, 3));
    CHECK(tool.release({20, 0}).done.empty()); // no pulling: nothing, the click follows
    CHECK(tool.busy());
}

TEST_CASE("sketch tool: an arc from a line with a typed radius", "[sketchtool][tangent]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    tool.click({20, 0});
    REQUIRE(tool.press({20, 0}, 5, 3));
    tool.moveTo({40, 20});
    tool.setTyped(0, 5.0);
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    const SketchElement& arc = step.done[0][0];
    checkPoint(arc.points[0], 20, 5);
    CHECK(std::hypot(arc.points[1].u - arc.points[0].u, arc.points[1].v - arc.points[0].v) ==
          Approx(5.0));
    CHECK_FALSE(tool.dragging());
    CHECK(tool.release({40, 20}).done.empty()); // the button comes up later: nothing more
}

TEST_CASE("sketch tool: a slot is four elements, typed length, angle and width",
          "[sketchtool][slot]") {
    SketchTool tool;
    tool.select(SketchToolKind::slot);
    CHECK(tool.click({0, 0}).done.empty());
    CHECK(tool.fieldCount() == 2); // length, angle
    CHECK(tool.click({10, 0}).done.empty());
    CHECK(tool.fieldCount() == 1); // width
    tool.moveTo({5, 3});
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 1);
    CHECK(draft.fields[0].name == "width");
    CHECK(draft.fields[0].value == Approx(6.0)); // the whole width, twice the distance
    const auto step = tool.click({5, 3});
    REQUIRE(step.done.size() == 1);
    CHECK(step.done[0].size() == 4); // one batch: one undo step
    CHECK(step.done[0][0].kind == SketchElementKind::line);
    CHECK(step.done[0][1].kind == SketchElementKind::arc);

    tool.click({0, 0});
    tool.moveTo({1, 1});
    tool.setTyped(0, 8.0);
    tool.setTyped(1, 90.0);
    CHECK(tool.accept().done.empty()); // Enter sets the second middle, the width follows
    CHECK(tool.placedCount() == 2);
    tool.moveTo({1, 1});
    tool.setTyped(0, 5.0);
    const auto typed = tool.accept();
    REQUIRE(typed.done.size() == 1);
    REQUIRE(typed.done[0].size() == 4);
    checkPoint(typed.done[0][1].points[0], 0, 8); // the middle of the round end
    CHECK(std::hypot(typed.done[0][1].points[1].u - 0, typed.done[0][1].points[1].v - 8) ==
          Approx(2.5));
}

TEST_CASE("sketch tool: a slot that is too short or too narrow is not drawn",
          "[sketchtool][slot]") {
    SketchTool tool;
    tool.select(SketchToolKind::slot);
    tool.click({0, 0});
    const auto tooShort = tool.click({0.001, 0});
    CHECK(tooShort.notice == SketchToolNotice::tooSmall);
    CHECK(tool.placedCount() == 1);
    tool.click({10, 0});
    const auto tooNarrow = tool.click({5, 0.001});
    CHECK(tooNarrow.done.empty());
    CHECK(tooNarrow.notice == SketchToolNotice::tooSmall);
    CHECK(tool.placedCount() == 2);
}

TEST_CASE("sketch tool: a polygon from the middle and a corner, six sides to begin with",
          "[sketchtool][polygon]") {
    SketchTool tool;
    tool.select(SketchToolKind::polygon);
    CHECK(tool.polygonSides() == 6);
    tool.click({0, 0});
    CHECK(tool.fieldCount() == 3);
    tool.moveTo({10, 0});
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 3);
    CHECK(draft.fields[0].name == "radius");
    CHECK(draft.fields[1].name == "sides");
    CHECK(draft.fields[1].value == Approx(6.0));
    CHECK(draft.fields[2].name == "angle");
    const auto step = tool.click({10, 0});
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 6);
    checkPoint(step.done[0][0].points[0], 10, 0);
    checkPoint(step.done[0][0].points[1], 5, 10 * std::sin(std::numbers::pi / 3));
    CHECK(step.done[0][5].points[1] == step.done[0][0].points[0]);
}

TEST_CASE("sketch tool: a polygon by the middle of a side has the inner circle as radius",
          "[sketchtool][polygon]") {
    SketchTool tool;
    tool.select(SketchToolKind::polygon, kSketchVariantSecond);
    tool.click({0, 0});
    tool.moveTo({0, 10}); // the middle of the top side is 10 away
    const auto step = tool.click({0, 10});
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 6);
    // the top side lies flat at v = 10, between two corners
    const double corner = 10.0 / std::cos(std::numbers::pi / 6.0);
    bool flatTop = false;
    for (const SketchElement& line : step.done[0]) {
        flatTop = flatTop || (std::abs(line.points[0].v - 10.0) < 1e-9 &&
                              std::abs(line.points[1].v - 10.0) < 1e-9);
        CHECK(std::hypot(line.points[0].u, line.points[0].v) == Approx(corner));
    }
    CHECK(flatTop);
}

TEST_CASE("sketch tool: the polygon takes 3, 6 and 64 sides, the last number is kept",
          "[sketchtool][polygon]") {
    SketchTool tool;
    tool.select(SketchToolKind::polygon);
    for (const int sides : {3, 64, 6}) {
        tool.click({0, 0});
        tool.moveTo({10, 0});
        CHECK(tool.setTyped(1, sides) == SketchToolNotice::none);
        const auto step = tool.accept();
        REQUIRE(step.done.size() == 1);
        CHECK(step.done[0].size() == static_cast<std::size_t>(sides));
        CHECK(tool.polygonSides() == sides);
    }
    tool.setPolygonSides(5);
    tool.click({0, 0});
    const auto five = tool.click({10, 0});
    REQUIRE(five.done.size() == 1);
    CHECK(five.done[0].size() == 5);
    tool.setPolygonSides(100); // clamped
    CHECK(tool.polygonSides() == 64);
}

TEST_CASE("sketch tool: a number of sides that is not 3 to 64 is refused, the old one stays",
          "[sketchtool][polygon]") {
    SketchTool tool;
    tool.select(SketchToolKind::polygon);
    tool.click({0, 0});
    tool.moveTo({10, 0});
    for (const double bad : {2.0, 65.0, 0.0, -6.0, 5.5}) {
        CHECK(tool.setTyped(1, bad) == SketchToolNotice::badSides);
        CHECK(tool.draft().fields[1].value == Approx(6.0));
        CHECK_FALSE(tool.draft().fields[1].typed);
    }
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    CHECK(step.done[0].size() == 6);
}

TEST_CASE("sketch tool: a typed polygon radius and angle", "[sketchtool][polygon]") {
    SketchTool tool;
    tool.select(SketchToolKind::polygon);
    tool.click({1, 1});
    tool.moveTo({3, 1});
    tool.setTyped(0, 4.0);
    tool.setTyped(1, 4.0);
    tool.setTyped(2, 45.0);
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 4);
    checkPoint(step.done[0][0].points[0], 1 + 4 * std::cos(std::numbers::pi / 4),
               1 + 4 * std::sin(std::numbers::pi / 4));

    tool.click({0, 0});
    tool.moveTo({0.001, 0});
    const auto tiny = tool.click({0.001, 0});
    CHECK(tiny.done.empty());
    CHECK(tiny.notice == SketchToolNotice::tooSmall);
}

// --- the curve (AP-2.1d) ---------------------------------------------------------------------

namespace {

SketchTool curveTool() {
    SketchTool tool;
    tool.select(SketchToolKind::spline);
    return tool;
}

} // namespace

TEST_CASE("sketch tool: a curve takes a click per point and a double click sets it",
          "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    CHECK(tool.click({0, 0}).done.empty());
    CHECK(tool.busy());
    CHECK(tool.click({10, 5}).done.empty());
    CHECK(tool.click({20, -5}).done.empty());
    CHECK(tool.placedCount() == 3);
    const auto step = tool.doubleClick(); // the click that came first has put its point
    REQUIRE(step.done.size() == 1);
    REQUIRE(step.done[0].size() == 1);
    const SketchElement& curve = step.done[0][0];
    CHECK(curve.kind == SketchElementKind::spline);
    REQUIRE(curve.points.size() == 3);
    checkPoint(curve.points[0], 0, 0);
    checkPoint(curve.points[1], 10, 5);
    checkPoint(curve.points[2], 20, -5);
    CHECK_FALSE(tool.busy()); // the next click starts another curve
    tool.click({3, 3});
    CHECK(tool.placedCount() == 1);
}

TEST_CASE("sketch tool: Enter sets the curve through the points so far", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 5});
    tool.moveTo({30, 30}); // the pointer is not a point yet
    const auto step = tool.accept();
    REQUIRE(step.done.size() == 1);
    CHECK(step.done[0][0].points.size() == 2);
    CHECK_FALSE(tool.busy());
}

TEST_CASE("sketch tool: the preview shows the curve to the pointer", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 5});
    tool.moveTo({20, 0});
    const SketchDraft draft = tool.draft();
    const auto curve = std::ranges::find_if(draft.elements, [](const SketchElement& element) {
        return element.kind == SketchElementKind::spline;
    });
    REQUIRE(curve != draft.elements.end());
    REQUIRE(curve->points.size() == 3);
    checkPoint(curve->points[2], 20, 0);
    // the points clicked so far show as dots
    CHECK(std::ranges::count_if(draft.elements, [](const SketchElement& element) {
              return element.kind == SketchElementKind::point;
          }) == 2);
    REQUIRE(draft.fields.size() == 2);
    CHECK(draft.fields[0].name == "length");
    CHECK(draft.fields[0].value == Approx(std::hypot(10.0, 5.0)));
}

TEST_CASE("sketch tool: Esc throws the unfinished curve away", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 5});
    tool.click({20, 0});
    CHECK(tool.cancel());
    CHECK_FALSE(tool.busy());
    CHECK(tool.placedCount() == 0);
    CHECK(tool.doubleClick().done.empty()); // nothing is left to set
    CHECK_FALSE(tool.cancel());             // the next Esc is for the tool (the controller)
    CHECK(tool.kind() == SketchToolKind::spline);
}

TEST_CASE("sketch tool: Backspace takes the last point back", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    CHECK_FALSE(tool.undoPoint());
    tool.click({0, 0});
    tool.click({10, 5});
    tool.click({20, 0});
    CHECK(tool.undoPoint());
    CHECK(tool.placedCount() == 2);
    checkPoint(tool.placedPoints().back(), 10, 5);
    CHECK(tool.undoPoint());
    CHECK(tool.undoPoint()); // the first point too: then nothing is pulled
    CHECK_FALSE(tool.busy());
    CHECK_FALSE(tool.undoPoint());
    // a point taken back can be put again somewhere else
    tool.click({1, 1});
    tool.click({5, 5});
    tool.undoPoint();
    tool.click({6, 0});
    const auto step = tool.doubleClick();
    REQUIRE(step.done.size() == 1);
    checkPoint(step.done[0][0].points[1], 6, 0);
}

TEST_CASE("sketch tool: other tools have no point to take back", "[sketchtool][spline]") {
    SketchTool tool;
    tool.select(SketchToolKind::line);
    tool.click({0, 0});
    CHECK_FALSE(tool.undoPoint());
    CHECK(tool.busy());
}

TEST_CASE("sketch tool: a click on the first point closes the curve and sets it",
          "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 0});
    tool.click({10, 10});
    tool.click({0, 10});
    const auto step = tool.click({0, 0});
    REQUIRE(step.done.size() == 1);
    const SketchElement& curve = step.done[0][0];
    CHECK(curve.kind == SketchElementKind::spline);
    REQUIRE(curve.points.size() == 5);
    CHECK(curve.points.front() == curve.points.back()); // closed = the last point is the first
    CHECK_FALSE(tool.busy());
}

TEST_CASE("sketch tool: the pointer on the first point shows the closed curve",
          "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 0});
    tool.click({10, 10});
    tool.moveTo({0, 0});
    const SketchDraft draft = tool.draft();
    const auto curve = std::ranges::find_if(draft.elements, [](const SketchElement& element) {
        return element.kind == SketchElementKind::spline;
    });
    REQUIRE(curve != draft.elements.end());
    REQUIRE(curve->points.size() == 4);
    CHECK(curve->points.front() == curve->points.back());
}

TEST_CASE("sketch tool: a curve of two points cannot close on its first", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 0});
    const auto step = tool.click({0, 0});
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
    CHECK(tool.placedCount() == 2); // the curve goes on
}

TEST_CASE("sketch tool: two points make a straight curve, one point nothing",
          "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 4});
    const auto two = tool.doubleClick();
    REQUIRE(two.done.size() == 1);
    CHECK(two.done[0][0].points.size() == 2);

    tool.click({5, 5});
    const auto one = tool.doubleClick();
    CHECK(one.done.empty());
    CHECK(one.notice == SketchToolNotice::tooSmall);
    CHECK_FALSE(tool.busy());
    tool.click({5, 5});
    const auto enter = tool.accept();
    CHECK(enter.done.empty());
    CHECK(enter.notice == SketchToolNotice::tooSmall);
}

TEST_CASE("sketch tool: points that repeat are one point when the curve is set",
          "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 0});
    tool.click({10, 0});         // a double click leaves the same point twice: it is taken once
    tool.click({10.004, 0.003}); // nearer than 0.01 mm
    tool.click({20, 5});
    CHECK(tool.placedCount() == 3);
    const auto step = tool.doubleClick();
    REQUIRE(step.done.size() == 1);
    CHECK(step.done[0][0].points.size() == 3);
}

TEST_CASE("sketch tool: only duplicates of one point make nothing", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({4, 4});
    tool.click({4, 4});
    tool.click({4.001, 4});
    CHECK(tool.placedCount() == 1);
    const auto step = tool.doubleClick();
    CHECK(step.done.empty());
    CHECK(step.notice == SketchToolNotice::tooSmall);
}

TEST_CASE("sketch tool: typing the length and angle gives the next point of the curve",
          "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    CHECK(tool.fieldCount() == 2);
    tool.moveTo({3, 1});
    tool.setTyped(0, 10.0);
    tool.setTyped(1, 90.0);
    const SketchDraft draft = tool.draft();
    REQUIRE(draft.fields.size() == 2);
    CHECK(draft.fields[0].typed);
    checkPoint(draft.at, 0, 10);
    const auto next = tool.accept(); // Enter with numbers adds the point ...
    CHECK(next.done.empty());
    CHECK(tool.placedCount() == 2);
    checkPoint(tool.placedPoints().back(), 0, 10);
    CHECK_FALSE(tool.hasTyped());
    tool.moveTo({20, 10});
    tool.setTyped(0, 5.0);
    tool.setTyped(1, 0.0);
    tool.accept();
    checkPoint(tool.placedPoints().back(), 5, 10);
    const auto step = tool.accept(); // ... and Enter again sets the curve
    REQUIRE(step.done.size() == 1);
    CHECK(step.done[0][0].points.size() == 3);
}

TEST_CASE("sketch tool: a helper switch makes the curve an aid", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.setHelperLine(true);
    tool.click({0, 0});
    tool.click({10, 5});
    const auto step = tool.doubleClick();
    REQUIRE(step.done.size() == 1);
    CHECK(step.done[0][0].construction);
}

TEST_CASE("sketch tool: choosing another tool drops the running curve", "[sketchtool][spline]") {
    SketchTool tool = curveTool();
    tool.click({0, 0});
    tool.click({10, 5});
    tool.select(SketchToolKind::line);
    CHECK_FALSE(tool.busy());
    tool.select(SketchToolKind::spline);
    CHECK(tool.placedCount() == 0);
}
