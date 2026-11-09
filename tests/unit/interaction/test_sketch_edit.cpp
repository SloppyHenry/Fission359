// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Trimming and extending (AP-2.1e1) as pure logic: the pieces of the elements between their
// crossings, which of them go, and how far an end can be extended.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "fission/geo/occt/kernel.h"
#include "fission/interaction/sketch_edit.h"

using Catch::Approx;
using namespace fission::interaction;
using fission::core::SketchElement;
using fission::core::SketchElementKind;
using fission::core::SketchPoint;

namespace {

constexpr double kPi = std::numbers::pi;

SketchElement line(std::string id, SketchPoint a, SketchPoint b) {
    return {std::move(id), SketchElementKind::line, {a, b}, false};
}
SketchElement circle(std::string id, SketchPoint middle, double radius) {
    return {
        std::move(id), SketchElementKind::circle, {middle, {middle.u + radius, middle.v}}, false};
}
// counter-clockwise from `from` to `to` degrees
SketchElement arc(std::string id, SketchPoint middle, double radius, double from, double to) {
    const double a = from * kPi / 180.0;
    const double b = to * kPi / 180.0;
    return {std::move(id),
            SketchElementKind::arc,
            {middle,
             {middle.u + radius * std::cos(a), middle.v + radius * std::sin(a)},
             {middle.u + radius * std::cos(b), middle.v + radius * std::sin(b)}},
            false};
}
SketchElement spline(std::string id, std::vector<SketchPoint> through) {
    return {std::move(id), SketchElementKind::spline, std::move(through), false};
}

void checkPoint(const SketchPoint& point, double u, double v) {
    CHECK(point.u == Approx(u).margin(1e-9));
    CHECK(point.v == Approx(v).margin(1e-9));
}

std::unique_ptr<fission::geo::GeometryKernel> kernel() {
    return fission::geo::occt::makeKernel();
}

} // namespace

TEST_CASE("trim: a line crossed twice has three pieces, found under the pointer",
          "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{
        line("L1", {0, 0}, {10, 0}), line("L2", {3, -4}, {3, 4}), line("L3", {7, -4}, {7, 4})};
    const SketchTrimmer trimmer(*k, sketch);
    REQUIRE(trimmer.usable());
    const auto left = trimmer.pieceAt({1, 0.05}, 0.5);
    const auto middle = trimmer.pieceAt({5, -0.05}, 0.5);
    const auto right = trimmer.pieceAt({9, 0}, 0.5);
    REQUIRE(left);
    REQUIRE(middle);
    REQUIRE(right);
    CHECK(left->element == "L1");
    CHECK(left->index == 0);
    CHECK(middle->index == 1);
    CHECK(right->index == 2);
    CHECK_FALSE(trimmer.pieceAt({5, 2}, 0.5).has_value()); // nothing within reach

    const auto outline = trimmer.outline(*middle);
    REQUIRE(outline.size() == 2);
    checkPoint(outline[0], 3, 0);
    checkPoint(outline[1], 7, 0);
}

TEST_CASE("trim: at a corner the element the stroke is on wins", "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                            line("L2", {10, 0}, {10, 8})};
    const SketchTrimmer trimmer(*k, sketch);
    CHECK(trimmer.pieceAt({10, 0}, 0.5)->element == "L1"); // equally near: the first
    CHECK(trimmer.pieceAt({10, 0}, 0.5, "L2")->element == "L2");
    CHECK(trimmer.pieceAt({10, 0}, 0.5, "L1")->element == "L1");
    CHECK(trimmer.pieceAt({10, 3}, 0.5, "L1")->element == "L2"); // nearer wins over the wish
}

TEST_CASE("trim: a piece in the middle makes two lines, the first keeps the id", "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{
        line("L1", {0, 0}, {10, 0}), line("L2", {3, -4}, {3, 4}), line("L3", {7, -4}, {7, 4})};
    const SketchTrimmer trimmer(*k, sketch);
    const SketchEdit edit = trimmer.trim({{"L1", 1}});
    REQUIRE(edit.moves.size() == 1);
    CHECK(edit.moves[0].element == "L1");
    checkPoint(edit.moves[0].points[0], 0, 0);
    checkPoint(edit.moves[0].points[1], 3, 0);
    REQUIRE(edit.added.size() == 1);
    CHECK(edit.added[0].kind == SketchElementKind::line);
    checkPoint(edit.added[0].points[0], 7, 0);
    checkPoint(edit.added[0].points[1], 10, 0);
    CHECK(edit.removed.empty());
    CHECK(edit.notice == SketchEditNotice::none);
}

TEST_CASE("trim: an end piece shortens the line to the crossing", "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                            line("L2", {3, -4}, {3, 4})};
    const SketchTrimmer trimmer(*k, sketch);
    SECTION("the end") {
        const SketchEdit edit = trimmer.trim({{"L1", 1}});
        REQUIRE(edit.moves.size() == 1);
        checkPoint(edit.moves[0].points[0], 0, 0);
        checkPoint(edit.moves[0].points[1], 3, 0);
        CHECK(edit.added.empty());
    }
    SECTION("the start") {
        const SketchEdit edit = trimmer.trim({{"L1", 0}});
        REQUIRE(edit.moves.size() == 1);
        checkPoint(edit.moves[0].points[0], 3, 0);
        checkPoint(edit.moves[0].points[1], 10, 0);
    }
}

TEST_CASE("trim: a line nothing crosses goes whole", "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                            line("L2", {20, -4}, {20, 4})};
    const SketchTrimmer trimmer(*k, sketch);
    const auto piece = trimmer.pieceAt({5, 0}, 0.5);
    REQUIRE(piece);
    const SketchEdit edit = trimmer.trim({*piece});
    CHECK(edit.removed == std::vector<std::string>{"L1"});
    CHECK(edit.moves.empty());
    CHECK(edit.added.empty());
}

TEST_CASE("trim: crossings at the ends do not cut", "[sketch][trim]") {
    const auto k = kernel();
    // a corner: the two lines meet at their ends, so each is one piece
    const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                            line("L2", {10, 0}, {10, 8})};
    const SketchTrimmer trimmer(*k, sketch);
    CHECK(trimmer.pieceAt({5, 0}, 0.5)->index == 0);
    const SketchEdit edit = trimmer.trim({{"L1", 0}});
    CHECK(edit.removed == std::vector<std::string>{"L1"});
}

TEST_CASE("trim: pieces of one stroke are taken together, against the sketch as it was",
          "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{
        line("L1", {0, 0}, {10, 0}), line("L2", {3, -4}, {3, 4}), line("L3", {7, -4}, {7, 4})};
    const SketchTrimmer trimmer(*k, sketch);
    SECTION("both ends: the middle stays") {
        const SketchEdit edit = trimmer.trim({{"L1", 0}, {"L1", 2}});
        REQUIRE(edit.moves.size() == 1);
        checkPoint(edit.moves[0].points[0], 3, 0);
        checkPoint(edit.moves[0].points[1], 7, 0);
        CHECK(edit.added.empty());
        CHECK(edit.removed.empty());
    }
    SECTION("all three pieces: the line goes") {
        const SketchEdit edit = trimmer.trim({{"L1", 0}, {"L1", 1}, {"L1", 2}});
        CHECK(edit.removed == std::vector<std::string>{"L1"});
        CHECK(edit.moves.empty());
    }
    SECTION("pieces of several elements") {
        const SketchEdit edit = trimmer.trim({{"L1", 2}, {"L2", 0}});
        CHECK(edit.moves.size() == 2); // L1 shortened to x = 7, L2 shortened to y = 0
        CHECK(edit.removed.empty());
    }
    SECTION("the same piece twice counts once") {
        const SketchEdit edit = trimmer.trim({{"L1", 2}, {"L1", 2}});
        REQUIRE(edit.moves.size() == 1);
        checkPoint(edit.moves[0].points[1], 7, 0);
    }
}

TEST_CASE("trim: a circle cut at two places becomes an arc", "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{circle("C1", {0, 0}, 5), line("L1", {0, -8}, {0, 8})};
    const SketchTrimmer trimmer(*k, sketch);
    // the right half (through (5, 0)) and the left half
    const auto right = trimmer.pieceAt({5, 0}, 0.5);
    const auto left = trimmer.pieceAt({-5, 0}, 0.5);
    REQUIRE(right);
    REQUIRE(left);
    CHECK(right->element == "C1");
    CHECK(right->index != left->index);

    const SketchEdit edit = trimmer.trim({*right});
    CHECK(edit.removed == std::vector<std::string>{"C1"});
    REQUIRE(edit.added.size() == 1);
    const SketchElement& rest = edit.added[0];
    CHECK(rest.kind == SketchElementKind::arc);
    // what stays is the left half, counter-clockwise from the top to the bottom
    checkPoint(rest.points[0], 0, 0);
    checkPoint(rest.points[1], 0, 5);
    checkPoint(rest.points[2], 0, -5);

    const auto outline = trimmer.outline(*right);
    CHECK(outline.size() > 20); // round, so many short lines
    checkPoint(outline.front(), 0, -5);
    checkPoint(outline.back(), 0, 5);
    for (const SketchPoint& point : outline) {
        CHECK(std::hypot(point.u, point.v) == Approx(5.0).margin(1e-9));
    }
}

TEST_CASE("trim: a circle cut by fewer than two crossings goes whole", "[sketch][trim]") {
    const auto k = kernel();
    SECTION("no crossing") {
        const SketchTrimmer trimmer(*k, {circle("C1", {0, 0}, 5)});
        const auto piece = trimmer.pieceAt({0, 5}, 0.5);
        REQUIRE(piece);
        CHECK(trimmer.trim({*piece}).removed == std::vector<std::string>{"C1"});
        CHECK(trimmer.outline(*piece).size() > 20);
    }
    SECTION("a touch is one crossing") {
        const SketchTrimmer trimmer(*k, {circle("C1", {0, 0}, 5), line("L1", {-8, 5}, {8, 5})});
        const SketchEdit edit = trimmer.trim({*trimmer.pieceAt({0, -5}, 0.5)});
        CHECK(edit.removed == std::vector<std::string>{"C1"});
        CHECK(edit.added.empty());
    }
}

TEST_CASE("trim: a circle with three crossings keeps the arcs that were not taken",
          "[sketch][trim]") {
    const auto k = kernel();
    // lines through the circle at 90 degrees (up), 210 and 330: three pieces
    const double r = 5.0;
    std::vector<SketchElement> sketch{circle("C1", {0, 0}, r)};
    int n = 0;
    for (const double degrees : {90.0, 210.0, 330.0}) {
        const double a = degrees * kPi / 180.0;
        sketch.push_back(line("L" + std::to_string(++n),
                              {r * 0.5 * std::cos(a), r * 0.5 * std::sin(a)},
                              {r * 1.5 * std::cos(a), r * 1.5 * std::sin(a)}));
    }
    const SketchTrimmer trimmer(*k, sketch);
    const auto first = trimmer.pieceAt({r, 0}, 0.3); // between 330 and 90
    REQUIRE(first);
    const SketchEdit edit = trimmer.trim({*first});
    CHECK(edit.removed == std::vector<std::string>{"C1"});
    REQUIRE(edit.added.size() == 1); // the other two pieces are next to each other: one arc
    checkPoint(edit.added[0].points[1], r * std::cos(90 * kPi / 180), r * std::sin(90 * kPi / 180));
    checkPoint(edit.added[0].points[2], r * std::cos(330 * kPi / 180),
               r * std::sin(330 * kPi / 180));
}

TEST_CASE("trim: an arc is cut like a line, on its circle", "[sketch][trim]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{arc("A1", {0, 0}, 5, 0, 180),
                                            line("L1", {0, 0}, {0, 8})};
    const SketchTrimmer trimmer(*k, sketch);
    const auto first = trimmer.pieceAt({5, 0.1}, 0.5);
    REQUIRE(first);
    CHECK(first->element == "A1");
    CHECK(first->index == 0);
    const SketchEdit edit = trimmer.trim({*first});
    REQUIRE(edit.moves.size() == 1);
    CHECK(edit.moves[0].element == "A1");
    checkPoint(edit.moves[0].points[0], 0, 0);
    checkPoint(edit.moves[0].points[1], 0, 5);  // the new start
    checkPoint(edit.moves[0].points[2], -5, 0); // the end stays
}

TEST_CASE("trim: a helper line is trimmed like any other and stays one", "[sketch][trim]") {
    const auto k = kernel();
    SketchElement helper = line("L1", {0, 0}, {10, 0});
    helper.construction = true;
    const SketchTrimmer trimmer(*k,
                                {helper, line("L2", {3, -4}, {3, 4}), line("L3", {7, -4}, {7, 4})});
    const SketchEdit edit = trimmer.trim({{"L1", 1}});
    REQUIRE(edit.added.size() == 1);
    CHECK(edit.added[0].construction);
}

TEST_CASE("trim: a curve goes whole when nothing crosses it, else it gives a hint",
          "[sketch][trim]") {
    const auto k = kernel();
    const SketchElement wave = spline("S1", {{0, 0}, {10, 8}, {20, -4}, {35, 6}});
    SECTION("nothing crosses it") {
        const SketchTrimmer trimmer(*k, {wave, line("L1", {0, 30}, {30, 30})});
        const auto piece = trimmer.pieceAt({10, 8}, 0.5);
        REQUIRE(piece);
        CHECK(piece->element == "S1");
        const SketchEdit edit = trimmer.trim({*piece});
        CHECK(edit.removed == std::vector<std::string>{"S1"});
        CHECK(edit.notice == SketchEditNotice::none);
        CHECK(trimmer.outline(*piece).size() >= 4);
    }
    SECTION("a line crosses it: nothing changes, a hint says why") {
        const SketchTrimmer trimmer(*k, {wave, line("L1", {15, -20}, {15, 20})});
        const SketchEdit edit = trimmer.trim({*trimmer.pieceAt({10, 8}, 0.5)});
        CHECK(edit.empty());
        CHECK(edit.notice == SketchEditNotice::curvePartial);
    }
    SECTION("in a stroke over a curve and a line the line is still trimmed") {
        const SketchTrimmer trimmer(
            *k, {wave, line("L1", {15, -20}, {15, 20}), line("L2", {0, 30}, {30, 30})});
        const SketchEdit edit = trimmer.trim({{"S1", 0}, {"L2", 0}});
        CHECK(edit.removed == std::vector<std::string>{"L2"});
        CHECK(edit.notice == SketchEditNotice::curvePartial);
    }
}

TEST_CASE("trim: points have no pieces", "[sketch][trim]") {
    const auto k = kernel();
    const SketchElement mark{"P1", SketchElementKind::point, {{3, 3}}, false};
    const SketchTrimmer trimmer(*k, {mark});
    CHECK_FALSE(trimmer.pieceAt({3, 3}, 1.0).has_value());
}

TEST_CASE("extend: a line goes on to the next curve, at either end", "[sketch][extend]") {
    const auto k = kernel();
    const std::vector<SketchElement> sketch{
        line("L1", {0, 0}, {10, 0}), line("L2", {25, -5}, {25, 5}), line("L3", {-8, -5}, {-8, 5})};
    SECTION("the end") {
        const auto found = findExtension(*k, sketch, {9.5, 0.1}, 1.0);
        REQUIRE(found);
        CHECK(found->element == "L1");
        CHECK(found->atEnd);
        checkPoint(found->target, 25, 0);
        REQUIRE(found->preview.size() == 2);
        checkPoint(found->preview[0], 10, 0);
        checkPoint(found->preview[1], 25, 0);
        const SketchEdit edit = extensionEdit(sketch, *found);
        REQUIRE(edit.moves.size() == 1);
        CHECK(edit.moves[0].element == "L1");
        checkPoint(edit.moves[0].points[0], 0, 0);
        checkPoint(edit.moves[0].points[1], 25, 0);
        CHECK(edit.added.empty());
        CHECK(edit.removed.empty());
    }
    SECTION("the start") {
        const auto found = findExtension(*k, sketch, {0.5, 0}, 1.0);
        REQUIRE(found);
        CHECK_FALSE(found->atEnd);
        checkPoint(found->target, -8, 0);
        const SketchEdit edit = extensionEdit(sketch, *found);
        checkPoint(edit.moves[0].points[0], -8, 0);
        checkPoint(edit.moves[0].points[1], 10, 0);
    }
}

TEST_CASE("extend: nothing in the way, or only what is there already", "[sketch][extend]") {
    const auto k = kernel();
    SECTION("no target") {
        const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0})};
        CHECK_FALSE(findExtension(*k, sketch, {10, 0}, 1.0).has_value());
    }
    SECTION("a wall that stops short of the line's way") {
        const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                                line("L2", {25, 2}, {25, 5})};
        CHECK_FALSE(findExtension(*k, sketch, {10, 0}, 1.0).has_value());
    }
    SECTION("the end touches a wall already: the next one counts") {
        const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                                line("L2", {10, -5}, {10, 5}),
                                                line("L3", {20, -5}, {20, 5})};
        const auto found = findExtension(*k, sketch, {9.8, 0}, 1.0);
        REQUIRE(found);
        checkPoint(found->target, 20, 0);
    }
    SECTION("a click far from every line and arc") {
        const std::vector<SketchElement> sketch{line("L1", {0, 0}, {10, 0}),
                                                line("L2", {25, -5}, {25, 5})};
        CHECK_FALSE(findExtension(*k, sketch, {5, 8}, 1.0).has_value());
    }
    SECTION("circles and curves are not extended") {
        const std::vector<SketchElement> sketch{circle("C1", {0, 0}, 5),
                                                line("L1", {20, -9}, {20, 9}),
                                                spline("S1", {{0, 20}, {10, 22}, {20, 20}})};
        CHECK_FALSE(findExtension(*k, sketch, {5, 0}, 1.0).has_value());
        CHECK_FALSE(findExtension(*k, sketch, {0, 20}, 1.0).has_value());
    }
}

TEST_CASE("extend: a line stops at a curve and at a circle in its way", "[sketch][extend]") {
    const auto k = kernel();
    SECTION("a circle: the nearer crossing") {
        const std::vector<SketchElement> sketch{line("L1", {0, 0}, {4, 0}),
                                                circle("C1", {20, 0}, 5)};
        const auto found = findExtension(*k, sketch, {4, 0}, 1.0);
        REQUIRE(found);
        checkPoint(found->target, 15, 0);
    }
    SECTION("a curve") {
        const std::vector<SketchElement> sketch{line("L1", {0, 0}, {4, 0}),
                                                spline("S1", {{30, -10}, {32, 0}, {30, 10}})};
        const auto found = findExtension(*k, sketch, {4, 0}, 1.0);
        REQUIRE(found);
        CHECK(found->target.u == Approx(32.0).margin(1e-6));
    }
}

TEST_CASE("extend: an arc goes on along its circle", "[sketch][extend]") {
    const auto k = kernel();
    const double r = 5.0;
    SECTION("at its end, up to a line") {
        const std::vector<SketchElement> sketch{arc("A1", {0, 0}, r, 0, 60),
                                                line("L1", {-2, -10}, {-2, 10})};
        const auto found =
            findExtension(*k, sketch, {r * std::cos(kPi / 3), r * std::sin(kPi / 3)}, 1.0);
        REQUIRE(found);
        CHECK(found->element == "A1");
        CHECK(found->atEnd);
        checkPoint(found->target, -2, std::sqrt(r * r - 4.0));
        CHECK(found->preview.size() > 5);
        for (const SketchPoint& point : found->preview) {
            CHECK(std::hypot(point.u, point.v) == Approx(r).margin(1e-9));
        }
        const SketchEdit edit = extensionEdit(sketch, *found);
        REQUIRE(edit.moves.size() == 1);
        checkPoint(edit.moves[0].points[0], 0, 0);
        checkPoint(edit.moves[0].points[1], r, 0); // the start stays
        checkPoint(edit.moves[0].points[2], -2, std::sqrt(r * r - 4.0));
    }
    SECTION("at its start, backwards, up to a line") {
        const std::vector<SketchElement> sketch{arc("A1", {0, 0}, r, 0, 60),
                                                line("L1", {-10, -3}, {10, -3})};
        const auto found = findExtension(*k, sketch, {r, 0.2}, 1.0);
        REQUIRE(found);
        CHECK_FALSE(found->atEnd);
        checkPoint(found->target, 4, -3);
        checkPoint(found->preview.front(), r, 0);
        checkPoint(found->preview.back(), 4, -3);
        const SketchEdit edit = extensionEdit(sketch, *found);
        checkPoint(edit.moves[0].points[1], 4, -3);
        checkPoint(edit.moves[0].points[2], r * std::cos(kPi / 3), r * std::sin(kPi / 3));
    }
    SECTION("no other curve in the way") {
        const std::vector<SketchElement> sketch{arc("A1", {0, 0}, r, 0, 60)};
        CHECK_FALSE(findExtension(*k, sketch, {r, 0.2}, 1.0).has_value());
    }
}
