// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Snapping (Fangen, AP-1.6c): the points of other bodies and of the print bed a moving body can
// catch on, and the rule that moves it onto one. Pure geometry (mm), no window: the controllers
// of the interface use it, and the tests check it.
//
// A moving body catches with the corners, edge middles and face middles of the box round it, on
// the points of the others: the corners, edge middles and centres of circles and flat faces. It
// is worked out axis by axis, so a body slides along an arrow and still lines up with a corner
// beside it; when one point of the body and one point of the others meet in every free axis at
// once, it is a point catch.

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "fission/geo/mesh.h"

namespace fission::interaction {

/// A point or an offset in the world (mm), x y z.
using Vec3 = std::array<double, 3>;

/// What a point is. The order is the order of preference when two are equally near.
enum class SnapKind { corner, circleCentre, edgeMiddle, faceMiddle };

struct SnapPoint {
    Vec3 position{};
    SnapKind kind = SnapKind::corner;
    bool operator==(const SnapPoint&) const = default;
};

/// How near (pixels on the screen) a point must be to catch.
inline constexpr double kSnapReachPixels = 10.0;

/// A coordinate of the display mesh (single precision) with the noise taken off: 12.30000019 reads
/// 12.3 again. Values that are not a multiple of 0.0001 mm within that noise stay as they are.
double cleanedCoordinate(double value);

/// The distance (mm) of the fine lines that show the snap grid: the grid step of the zoom
/// (`snapStep`), or a coarser one from the 1-2-5 series where the lines would be closer than
/// 8 pixels on the screen.
double snapGridDisplayStep(double pixelsPerMillimetre);

/// The points of a body a moving body can catch on: its corners, the middles of its open edges,
/// the centres of its circles and the middles of its flat faces. (Round faces have none: their
/// centre is not on them.)
std::vector<SnapPoint> snapPointsOf(const geo::TriangleMesh& mesh);

/// The 8 corners, 12 edge middles and 6 face middles of a box.
std::vector<SnapPoint> snapPointsOfBox(const Vec3& low, const Vec3& high);

/// The same for the top of the print bed (z = 0, origin in the middle): 4 corners, 4 edge middles
/// and the middle.
std::vector<SnapPoint> snapPointsOfBed(double sizeX, double sizeY);

/// A catch on one axis: the point of the moving body and the point it caught on, both where they
/// are after the move.
struct SnapMatch {
    SnapPoint mover;
    SnapPoint target;
    /// Mover and target are the same place: the body sits on the point (the mark is a point mark;
    /// otherwise the two are lined up and joined by a line).
    bool onPoint() const;
};

struct SnapResult {
    Vec3 offset{};                                ///< the move after catching
    std::array<std::optional<SnapMatch>, 3> axes; ///< the catch per axis (x, y, z)
    bool caught() const { return axes[0] || axes[1] || axes[2]; }
};

/// Moves `offset` onto the nearest catch. `movers` are the points of the moving body where it
/// stood when the move began, `targets` the points to catch on. Only axes marked `free` change;
/// `reach` is how far (mm) a point may be off on each axis and still catch. A point that meets a
/// target in every free axis at once is preferred; otherwise each axis catches on its own.
SnapResult snapOffset(const std::vector<SnapPoint>& movers, const std::vector<SnapPoint>& targets,
                      const Vec3& offset, const std::array<bool, 3>& free, const Vec3& reach);

} // namespace fission::interaction
