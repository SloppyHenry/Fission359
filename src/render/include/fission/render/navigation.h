// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Which mouse gesture moves the camera how (docs/02-ux-konzept.md §10). The styles imitate the
// programs people come from, so their hands already know what to do.

#pragma once

#include <Qt>

#include <array>
#include <optional>
#include <string_view>

namespace fission::render {

enum class MouseStyle { fission, fusion, tinkercad, blender, sketchup };

enum class NavigationAction { none, orbit, pan, zoom };

inline constexpr std::array<MouseStyle, 5> kMouseStyles{MouseStyle::fission, MouseStyle::fusion,
                                                        MouseStyle::tinkercad, MouseStyle::blender,
                                                        MouseStyle::sketchup};

/// Stable name used in settings and QML ("fission", "fusion", ...).
std::string_view mouseStyleName(MouseStyle style);
std::optional<MouseStyle> mouseStyleFromName(std::string_view name);

/// What dragging with `buttons` held and `modifiers` pressed does. Only the three navigation
/// buttons count; Alt+left and Shift+left always work as a fallback for laptops without a
/// middle button. The mouse wheel zooms to the pointer in every style.
NavigationAction dragAction(MouseStyle style, Qt::MouseButtons buttons,
                            Qt::KeyboardModifiers modifiers);

/// What two fingers on a trackpad do with the given modifiers: turn, with Shift move, and
/// with Ctrl (Blender users) zoom.
NavigationAction trackpadAction(MouseStyle style, Qt::KeyboardModifiers modifiers);

} // namespace fission::render
