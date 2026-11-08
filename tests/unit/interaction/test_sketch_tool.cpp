// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The drawing tools of the sketch mode (AP-2.1c1) as a state machine: line chain, rectangle,
// circle, point; typed numbers; helper lines; elements that are too small.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

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
