// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "fission/render/navigation.h"

using namespace fission::render;

namespace {

struct Gesture {
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    NavigationAction expected;
};

void check(MouseStyle style, std::initializer_list<Gesture> gestures) {
    for (const Gesture& gesture : gestures) {
        INFO("style " << mouseStyleName(style) << ", buttons " << int(gesture.buttons)
                      << ", modifiers " << int(gesture.modifiers));
        CHECK(dragAction(style, gesture.buttons, gesture.modifiers) == gesture.expected);
    }
}

} // namespace

TEST_CASE("every mouse style turns, moves and zooms as its program does", "[navigation]") {
    using A = NavigationAction;
    const auto left = Qt::LeftButton;
    const auto middle = Qt::MiddleButton;
    const auto right = Qt::RightButton;
    const auto none = Qt::NoModifier;
    const auto shift = Qt::ShiftModifier;
    const auto ctrl = Qt::ControlModifier;

    check(MouseStyle::fission, {{right, none, A::orbit},
                                {right, shift, A::pan},
                                {middle, none, A::pan},
                                {middle, shift, A::none},
                                {left, none, A::none}});
    check(MouseStyle::fusion, {{middle, none, A::pan},
                               {middle, shift, A::orbit},
                               {right, none, A::none},
                               {left, none, A::none}});
    check(MouseStyle::tinkercad, {{right, none, A::orbit},
                                  {right, shift, A::pan},
                                  {middle, none, A::pan},
                                  {left, none, A::none}});
    check(MouseStyle::blender, {{middle, none, A::orbit},
                                {middle, shift, A::pan},
                                {middle, ctrl, A::zoom},
                                {right, none, A::none}});
    check(MouseStyle::sketchup,
          {{middle, none, A::orbit}, {middle, shift, A::pan}, {right, none, A::none}});
}

TEST_CASE("Alt and Shift with the left button work in every style (no middle button)",
          "[navigation]") {
    for (const MouseStyle style : kMouseStyles) {
        CHECK(dragAction(style, Qt::LeftButton, Qt::AltModifier) == NavigationAction::orbit);
        CHECK(dragAction(style, Qt::LeftButton, Qt::ShiftModifier) == NavigationAction::pan);
        CHECK(dragAction(style, Qt::LeftButton, Qt::NoModifier) == NavigationAction::none);
        CHECK(dragAction(style, Qt::NoButton, Qt::NoModifier) == NavigationAction::none);
    }
}

TEST_CASE("the trackpad turns with two fingers, Shift moves", "[navigation]") {
    for (const MouseStyle style : kMouseStyles) {
        CHECK(trackpadAction(style, Qt::NoModifier) == NavigationAction::orbit);
        CHECK(trackpadAction(style, Qt::ShiftModifier) == NavigationAction::pan);
    }
    CHECK(trackpadAction(MouseStyle::blender, Qt::ControlModifier) == NavigationAction::zoom);
    CHECK(trackpadAction(MouseStyle::fusion, Qt::ControlModifier) == NavigationAction::none);
}

TEST_CASE("style names survive a round trip", "[navigation]") {
    for (const MouseStyle style : kMouseStyles) {
        CHECK(mouseStyleFromName(mouseStyleName(style)) == style);
    }
    CHECK_FALSE(mouseStyleFromName("maya").has_value());
}
