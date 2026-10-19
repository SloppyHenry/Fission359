// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fission/interaction/snap.h"
#include "fission/render/scene.h"
#include "fission/render/scene_renderer.h"

namespace fission::ui {

/// Snapping for one drag (AP-1.6c): knows the points to catch on (the corners, edge middles,
/// circle centres and face middles of the other bodies, and the print bed) and the points of the
/// body being moved, and turns the pull of the pointer into a move that catches on them. Also
/// draws what it caught: a mark on the point and, where two points only line up, a dashed line
/// between them.
///
/// The points of a body are kept as long as its mesh stays the same, so a second drag does not
/// work them out again.
class SnapSession {
public:
    /// Starts a drag of the body `movingBody`; `low` and `high` are the corners of the box round
    /// it where it stands. The points of the other bodies come from `scene`.
    void begin(const render::RenderScene& scene, const std::string& movingBody,
               const interaction::Vec3& low, const interaction::Vec3& high);
    void end();
    bool active() const noexcept { return m_active; }

    /// Moves `offset` (mm, from where the drag began) onto the nearest catch.
    interaction::SnapResult snap(const interaction::Vec3& offset, const std::array<bool, 3>& free,
                                 const interaction::Vec3& reach) const;

    /// The picture of a catch. `mmPerPixel` says how long a pixel is where the marks are.
    static render::HandleGraphics marksOf(const interaction::SnapResult& result,
                                          const QColor& colour, double mmPerPixel);

    std::size_t targetCount() const noexcept { return m_targets.size(); }

private:
    struct Points {
        std::uint64_t version = 0;
        std::vector<interaction::SnapPoint> points;
    };
    std::map<std::string, Points> m_cache;
    std::vector<interaction::SnapPoint> m_targets;
    std::vector<interaction::SnapPoint> m_movers;
    bool m_active = false;
};

} // namespace fission::ui
