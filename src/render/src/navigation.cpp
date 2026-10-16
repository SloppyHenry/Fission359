// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/navigation.h"

namespace fission::render {

namespace {

constexpr Qt::KeyboardModifiers kRelevant =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;

struct Binding {
    Qt::MouseButton button;
    Qt::KeyboardModifiers modifiers;
    NavigationAction action;
};

constexpr Binding kFission[] = {
    {Qt::RightButton, Qt::NoModifier, NavigationAction::orbit},
    {Qt::RightButton, Qt::ShiftModifier, NavigationAction::pan},
    {Qt::MiddleButton, Qt::NoModifier, NavigationAction::pan},
};

// Fusion: middle button moves the view, Shift + middle button turns it.
constexpr Binding kFusion[] = {
    {Qt::MiddleButton, Qt::NoModifier, NavigationAction::pan},
    {Qt::MiddleButton, Qt::ShiftModifier, NavigationAction::orbit},
};

// Tinkercad: right button turns, Shift + right button or the middle button moves.
constexpr Binding kTinkercad[] = {
    {Qt::RightButton, Qt::NoModifier, NavigationAction::orbit},
    {Qt::RightButton, Qt::ShiftModifier, NavigationAction::pan},
    {Qt::MiddleButton, Qt::NoModifier, NavigationAction::pan},
};

// Blender: middle button turns, Shift moves, Ctrl zooms.
constexpr Binding kBlender[] = {
    {Qt::MiddleButton, Qt::NoModifier, NavigationAction::orbit},
    {Qt::MiddleButton, Qt::ShiftModifier, NavigationAction::pan},
    {Qt::MiddleButton, Qt::ControlModifier, NavigationAction::zoom},
};

// SketchUp: middle button turns, Shift + middle button moves.
constexpr Binding kSketchup[] = {
    {Qt::MiddleButton, Qt::NoModifier, NavigationAction::orbit},
    {Qt::MiddleButton, Qt::ShiftModifier, NavigationAction::pan},
};

template <std::size_t N>
NavigationAction lookup(const Binding (&table)[N], Qt::MouseButton button,
                        Qt::KeyboardModifiers modifiers) {
    for (const Binding& binding : table) {
        if (binding.button == button && binding.modifiers == modifiers) {
            return binding.action;
        }
    }
    return NavigationAction::none;
}

struct StyleName {
    MouseStyle style;
    std::string_view name;
};

constexpr StyleName kNames[] = {{MouseStyle::fission, "fission"},
                                {MouseStyle::fusion, "fusion"},
                                {MouseStyle::tinkercad, "tinkercad"},
                                {MouseStyle::blender, "blender"},
                                {MouseStyle::sketchup, "sketchup"}};

} // namespace

std::string_view mouseStyleName(MouseStyle style) {
    for (const StyleName& entry : kNames) {
        if (entry.style == style) {
            return entry.name;
        }
    }
    return "fission";
}

std::optional<MouseStyle> mouseStyleFromName(std::string_view name) {
    for (const StyleName& entry : kNames) {
        if (entry.name == name) {
            return entry.style;
        }
    }
    return std::nullopt;
}

NavigationAction dragAction(MouseStyle style, Qt::MouseButtons buttons,
                            Qt::KeyboardModifiers modifiers) {
    const Qt::KeyboardModifiers mods = modifiers & kRelevant;
    Qt::MouseButton button = Qt::NoButton;
    if (buttons.testFlag(Qt::MiddleButton)) {
        button = Qt::MiddleButton;
    } else if (buttons.testFlag(Qt::RightButton)) {
        button = Qt::RightButton;
    } else if (buttons.testFlag(Qt::LeftButton)) {
        // Laptop fallback in every style: Alt + left turns, Shift + left moves.
        if (mods == Qt::AltModifier) {
            return NavigationAction::orbit;
        }
        if (mods == Qt::ShiftModifier) {
            return NavigationAction::pan;
        }
        return NavigationAction::none;
    }
    switch (style) {
    case MouseStyle::fission:
        return lookup(kFission, button, mods);
    case MouseStyle::fusion:
        return lookup(kFusion, button, mods);
    case MouseStyle::tinkercad:
        return lookup(kTinkercad, button, mods);
    case MouseStyle::blender:
        return lookup(kBlender, button, mods);
    case MouseStyle::sketchup:
        return lookup(kSketchup, button, mods);
    }
    return NavigationAction::none;
}

NavigationAction trackpadAction(MouseStyle style, Qt::KeyboardModifiers modifiers) {
    const Qt::KeyboardModifiers mods = modifiers & kRelevant;
    if (mods == Qt::ShiftModifier) {
        return NavigationAction::pan;
    }
    if (mods == Qt::ControlModifier && style == MouseStyle::blender) {
        return NavigationAction::zoom;
    }
    if (mods == Qt::NoModifier) {
        return NavigationAction::orbit;
    }
    return NavigationAction::none;
}

} // namespace fission::render
