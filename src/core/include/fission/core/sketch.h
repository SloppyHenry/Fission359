// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What a sketch is made of (AP-2.1a, ADR-0013): a plane and a list of elements (points, lines,
// circles, arcs, splines) with stable ids, in plane coordinates (u, v) in millimetres. Plain data
// and plain maths: no Qt, no kernel. The step type `sketch` carries a `SketchData`.

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fission/geo/rotation.h"
#include "fission/geo/types.h"

namespace fission::core {

/// A position on the sketch plane: u along the plane's first axis, v along its second (mm).
struct SketchPoint {
    double u = 0.0;
    double v = 0.0;

    bool operator==(const SketchPoint&) const = default;
};

/// A plane in space with its own coordinates: a point (the origin) and two perpendicular unit
/// vectors, `uAxis` and `vAxis`. The normal is `uAxis × vAxis`. Plane coordinates (u, v) and world
/// coordinates are never mixed: every crossing goes through `toWorld` and `toPlane`.
class SketchPlane {
public:
    /// The ground plane (u = X, v = Y, normal = Z) through the world origin.
    SketchPlane() = default;
    /// `uAxis` is normalised; `vAxis` is made perpendicular to it and normalised. Two axes that
    /// are zero or parallel give the ground plane's axes (there is no plane to build).
    SketchPlane(const geo::Point3d& origin, const geo::Point3d& uAxis, const geo::Point3d& vAxis);

    const geo::Point3d& origin() const noexcept { return origin_; }
    const geo::Point3d& uAxis() const noexcept { return u_; }
    const geo::Point3d& vAxis() const noexcept { return v_; }
    const geo::Point3d& normal() const noexcept { return n_; }

    /// The place in space of a point of the plane.
    geo::Point3d toWorld(const SketchPoint& point) const noexcept;
    /// The same for a direction (no origin).
    geo::Point3d directionToWorld(const SketchPoint& direction) const noexcept;
    /// The point of the plane nearest to a place in space (it is projected along the normal).
    SketchPoint toPlane(const geo::Point3d& world) const noexcept;
    /// How far a place in space is in front of the plane (along the normal), negative behind.
    double heightOf(const geo::Point3d& world) const noexcept;

    bool operator==(const SketchPlane&) const = default;

private:
    geo::Point3d origin_{0.0, 0.0, 0.0};
    geo::Point3d u_{1.0, 0.0, 0.0};
    geo::Point3d v_{0.0, 1.0, 0.0};
    geo::Point3d n_{0.0, 0.0, 1.0};
};

/// The three planes through the origin of the bed.
enum class BasePlane {
    xy, ///< Seen from above: u = X (right), v = Y (back), normal up.
    xz, ///< Seen from the front: u = X (right), v = Z (up), normal towards the viewer.
    yz  ///< Seen from the right: u = Y, v = Z (up), normal to the right.
};

/// The word of a base plane as stored in the step ("xy", "xz", "yz"), and back.
std::string_view basePlaneWord(BasePlane plane);
std::optional<BasePlane> basePlaneFromWord(std::string_view word);

/// A base plane moved to `origin` and turned by `turn` (degrees about the X, Y, Z axis of the bed,
/// as for bodies, ADR-0010) about that origin.
SketchPlane placedBasePlane(BasePlane plane, const geo::Point3d& origin, const geo::Angles& turn);

enum class SketchElementKind { point, line, circle, arc, spline };

inline constexpr std::size_t sketchElementKindCount = 5;

/// One thing in a sketch. All geometry is points, so that moving a point is the only way to
/// change an element and one command covers all of them:
///  - point:  `points` = {the point}
///  - line:   {start, end}
///  - circle: {middle, a point on the circle}; the radius is the distance between the two
///  - arc:    {middle, start, end}: from start to end counter-clockwise about the middle; the
///            radius is the distance middle-start, `end` only gives the direction
///  - spline: {the points it goes through, in order} (two or more)
/// A line, circle or arc with `construction` set is an aid ("Hilfslinie"): drawn dashed, never
/// forms an area.
struct SketchElement {
    std::string id; ///< "P1", "L3", "C2", "A1", "S1": letter of the kind + number (never reused).
    SketchElementKind kind = SketchElementKind::line;
    std::vector<SketchPoint> points;
    bool construction = false;

    bool operator==(const SketchElement&) const = default;
};

/// The letter an id of this kind starts with.
char sketchIdLetter(SketchElementKind kind) noexcept;
/// How many points a kind takes at least, and whether it takes more (only the spline does).
std::size_t sketchMinPoints(SketchElementKind kind) noexcept;
bool sketchTakesMorePoints(SketchElementKind kind) noexcept;
/// The right number of points for the kind, and every coordinate a finite number.
bool sketchPointsUsable(SketchElementKind kind, const std::vector<SketchPoint>& points) noexcept;

/// A rule or measure of the sketch (AP-2.2). Not used yet: the list is empty. The shape is fixed
/// so that files and ids already have their place; a measure's value (a formula) joins as an
/// optional field with AP-2.2.
struct SketchRule {
    std::string id; ///< "R1", ... counted up per sketch, never reused.
    std::string kind;
    std::vector<std::string> elements; ///< Ids of the elements it concerns.

    bool operator==(const SketchRule&) const = default;
};

/// The content of a `sketch` step beyond its plane (the plane is in the step's values, words and
/// references, see ADR-0013).
struct SketchData {
    std::vector<SketchElement> elements;
    /// The number each kind of id got last, indexed by `SketchElementKind`. Removing an element
    /// never frees its number.
    std::array<int, sketchElementKindCount> lastNumber{};
    std::vector<SketchRule> rules;
    int lastRuleNumber = 0;

    /// The next free id of a kind, counted up ("L1", "L2", ...).
    std::string allocateId(SketchElementKind kind);
    const SketchElement* find(std::string_view id) const noexcept;
    SketchElement* find(std::string_view id) noexcept;
    std::optional<std::size_t> indexOf(std::string_view id) const noexcept;

    /// True when ids are letter + number of their kind, none repeats, every number is within the
    /// counter of its kind (so a new id can never repeat one), and every element is usable.
    bool consistent() const;

    /// Everything that matters for a calculation, as text: equal text, equal sketch.
    std::string fingerprint() const;

    bool operator==(const SketchData&) const = default;
};

/// The id's number ("L12" -> 12) and kind; nullopt when it is not of that form.
struct ParsedSketchId {
    SketchElementKind kind = SketchElementKind::point;
    int number = 0;
};
std::optional<ParsedSketchId> parseSketchId(std::string_view id) noexcept;

} // namespace fission::core
