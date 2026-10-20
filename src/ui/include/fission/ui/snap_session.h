// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QVector3D>

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

class Viewport3D;

/// Snapping is off: Shift is held, or it was switched off for a test (`--simulate "snap:off"`).
/// Every drag that snaps asks this one place; a typed number is never snapped anyway.
bool snappingSuppressed();
/// Switches snapping off (or on again) as if Shift were held. For the tests and the developer
/// option `--simulate`; a test puts it back when it is done.
void setSnappingSuppressedForTests(bool suppressed);

/// How many millimetres a pixel is across the line of sight at `point`.
double millimetresPerPixelAt(const Viewport3D& viewport, const QVector3D& point);
/// How far (mm) a point near `around` may be off on each world axis and still catch: 10 pixels
/// on the screen (`interaction::kSnapReachPixels`). An axis that runs towards the viewer shows
/// hardly any of its millimetres, which must not make the reach huge.
interaction::Vec3 snapReachAround(const Viewport3D& viewport, const QVector3D& around);

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
    /// The same for a drag that moves something else than the whole body: `movers` are the points
    /// that catch (the points of one side of the box for a size handle, the underside of a shape
    /// put down), where they stand when the drag begins.
    void beginWithMovers(const render::RenderScene& scene, const std::string& movingBody,
                         std::vector<interaction::SnapPoint> movers);
    /// Replaces the points that catch while the drag goes on (the size of a shape put down is
    /// known a moment after it appeared).
    void setMovers(std::vector<interaction::SnapPoint> movers) { m_movers = std::move(movers); }
    void end();
    bool active() const noexcept { return m_active; }
    const std::vector<interaction::SnapPoint>& targets() const noexcept { return m_targets; }
    bool hasMovers() const noexcept { return !m_movers.empty(); }

    /// Moves `offset` (mm, from where the drag began) onto the nearest catch.
    interaction::SnapResult snap(const interaction::Vec3& offset, const std::array<bool, 3>& free,
                                 const interaction::Vec3& reach) const;

    /// The picture of a catch. `mmPerPixel` says how long a pixel is where the marks are.
    static render::HandleGraphics marksOf(const interaction::SnapResult& result,
                                          const QColor& colour, double mmPerPixel);

    std::size_t targetCount() const noexcept { return m_targets.size(); }

private:
    void collectTargets(const render::RenderScene& scene, const std::string& movingBody);

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
