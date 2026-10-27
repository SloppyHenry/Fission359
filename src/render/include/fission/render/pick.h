// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What the pick pass draws and how its picture is read (AP-1.4b). The pass renders every face,
// edge and corner with a number instead of a colour into a tiny picture around the pointer; the
// functions here pack those numbers and decide which element the pointer means. They know
// nothing of the graphics card and are tested without one.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "fission/core/element_names.h"
#include "fission/core/selection.h"

namespace fission::render {

/// The pick picture shows (2 × kPickRadius + 1)² screen pixels around the pointer, one pixel of
/// the picture per screen pixel.
inline constexpr int kPickRadius = 7;
inline constexpr int kPickSize = 2 * kPickRadius + 1;

/// How near (in screen pixels) the pointer must be to take an edge or a corner. A corner wins
/// against an edge that is up to `kCornerPreference` pixels nearer, because corners are small.
inline constexpr float kEdgeReach = 4.5F;
inline constexpr float kCornerReach = 6.0F;
inline constexpr float kCornerPreference = 2.5F;
/// Half the side of the square a corner is drawn with in the pick picture, in pixels.
inline constexpr float kCornerHalfSize = 4.5F;

/// Bodies are told apart in the picture by a slot number (not by their name); slot 0 is "none".
inline constexpr unsigned kPickSlots = 1024;
inline constexpr std::uint32_t kPickMaxNumber = (1U << 20) - 1;

/// One 32 bit number per drawn element: 2 bits kind (1 face, 2 edge, 3 corner), 10 bits slot of
/// the body, 20 bits number of the face, edge or corner in the body. 0 is "nothing here".
std::uint32_t packPickId(core::SelectableKind kind, unsigned slot, std::uint32_t number) noexcept;

struct PickId {
    core::SelectableKind kind = core::SelectableKind::body;
    unsigned slot = 0;
    std::uint32_t number = 0;
    bool valid = false; ///< false for 0, "nothing here"
};
PickId unpackPickId(std::uint32_t id) noexcept;

/// What the pointer points at: an element of the body in `slot` (or the body itself).
struct PickedElement {
    unsigned slot = 0;
    core::SelectableKind kind = core::SelectableKind::body;
    std::uint32_t number = 0; ///< 0 for a body
    float distance = 0.0F;    ///< in pixels from the pointer (0 for a face)
};

/// Decides from the picture (`kPickSize`² numbers, top row first, the pointer in the middle)
/// what the person means: the nearest corner or edge within reach, otherwise the face under
/// the pointer. Kinds the filter does not allow are skipped; a face that cannot be chosen counts
/// as its body if bodies can. (The pass does not even draw edges or corners the filter does not
/// allow, so that the face beneath them shows.)
std::optional<PickedElement> choosePicked(std::span<const std::uint32_t> picture,
                                          const core::SelectionFilter& filter);

/// Everything visible in a rectangle (a rubber band), one entry per element: bodies if they can
/// be chosen, otherwise faces, otherwise edges, otherwise corners. In the order of first sight.
std::vector<PickedElement> chooseInRegion(std::span<const std::uint32_t> picture,
                                          const core::SelectionFilter& filter);

} // namespace fission::render
