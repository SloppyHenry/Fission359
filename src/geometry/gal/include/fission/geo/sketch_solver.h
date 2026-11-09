// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The sketch solver behind the geometry abstraction layer (AP-2.2a, ADR-0013 addendum 2.2a).
// A sketch is handed over as plain data: points with coordinates, curves made of points, rules
// and measures that name points and curves by id. The solver moves the points until the rules and
// measures hold and says how much is still free, which rules contradict each other and which are
// redundant. Nothing here knows the constraint library behind it (PlaneGCS lives in
// `src/geometry/sketch`), and nothing knows the document: the caller turns a sketch step into a
// `SketchSystem` and writes the new coordinates back (AP-2.2b).

#pragma once

#include <string>
#include <vector>

#include "fission/geo/outline.h"

namespace fission::geo {

/// A point of the sketch on the sketch plane (mm). Every coordinate the solver may move is a point;
/// the points of a line, circle or arc, the through-points of a spline and stand-alone points all
/// look the same. Two curves may share a point (a joint).
struct SolverPoint {
    std::string id;
    Point2d position;

    bool operator==(const SolverPoint&) const = default;
};

enum class SolverCurveKind {
    line,   ///< points = {start, end}
    circle, ///< points = {centre, a point on the circle}; the radius is the distance of the two
    arc     ///< points = {centre, start, end}, counter-clockwise from start to end
};

/// A line, circle or arc, made of points by id. For an arc the radius is the distance centre to
/// start; the end only gives the direction, and a solved arc always has its end on the circle (an
/// end that lies off the circle is moved radially onto it). A spline takes part only through its
/// points (as stand-alone points); rules on the curve itself are not available.
struct SolverCurve {
    std::string id;
    SolverCurveKind kind = SolverCurveKind::line;
    std::vector<std::string> points;

    bool operator==(const SolverCurve&) const = default;
};

/// The twelve rules. `elements` are ids of points or curves; the form each rule takes:
///  - horizontal, vertical: {line} or {point, point}
///  - parallel, perpendicular: {line, line}
///  - tangent: {line, circle|arc}, {circle|arc, circle|arc} (the curves touch, the full
///    circle of an arc counts; curves that share an end point touch in that point and keep the
///    side they are on, internal or external)
///  - equal: {line, line} (same length) or {circle|arc, circle|arc} (same radius)
///  - symmetric: {point, point, line} (mirrored in the line) or {point, point, point} (mirrored in
///    the last point)
///  - midpoint: {point, line} (the point is the middle of the line)
///  - fixed: one or more points or curves; they stay where they are (all their points)
///  - pointOnCurve: {point, line|circle|arc} (on the line's infinite extension, on the full circle)
///  - concentric: {circle|arc, circle|arc} (same centre)
///  - coincident: {point, point}
enum class SketchRuleKind {
    horizontal,
    vertical,
    parallel,
    perpendicular,
    tangent,
    equal,
    symmetric,
    midpoint,
    fixed,
    pointOnCurve,
    concentric,
    coincident
};

/// The kinds of measures (the number is `value`):
///  - length: {line} length in mm (> 0), or {arc} arc length in mm (> 0)
///  - distance: {point, point} straight distance (> 0); {point, line} distance to the line
///    (> 0); {line, line} distance of two parallel lines (> 0)
///  - horizontalDistance / verticalDistance: {point, point}: coordinate of the second minus the one
///    of the first along u / v, in mm; any sign, zero allowed
///  - radius, diameter: {circle|arc} in mm (> 0)
///  - angle: {line}: direction of the line (start to end) from the u axis, counter-clockwise;
///    {line, line}: from the first line's direction to the second's, counter-clockwise. In degrees,
///    any finite value.
enum class SketchMeasureKind {
    length,
    distance,
    horizontalDistance,
    verticalDistance,
    radius,
    diameter,
    angle
};

/// A rule of the sketch. Rules and measures share one set of ids ("R1", "R2", ...): `conflicting`
/// and `redundant` of the result name either.
struct SolverRule {
    std::string id;
    SketchRuleKind kind = SketchRuleKind::horizontal;
    std::vector<std::string> elements;

    bool operator==(const SolverRule&) const = default;
};

struct SolverMeasure {
    std::string id;
    SketchMeasureKind kind = SketchMeasureKind::length;
    std::vector<std::string> elements;
    double value = 0.0; ///< mm, or degrees for `angle`

    bool operator==(const SolverMeasure&) const = default;
};

/// A point the person drags: the solver tries to bring it to `target`, as far as the rules allow
/// (the rules always win).
struct HeldPoint {
    std::string point;
    Point2d target;

    bool operator==(const HeldPoint&) const = default;
};

struct SketchSystem {
    std::vector<SolverPoint> points;
    std::vector<SolverCurve> curves;
    std::vector<SolverRule> rules;
    std::vector<SolverMeasure> measures;
    std::vector<HeldPoint> held;
};

struct SolveOptions {
    /// Analyse the system (degrees of freedom, contradictions, redundancy). Switching it off (live
    /// dragging) leaves `dof` at -1 and cannot report contradictions, only a failure.
    bool diagnose = true;
    /// Try a second algorithm when the first does not find a solution.
    bool fallback = true;
};

enum class SolveStatus {
    ok,       ///< every rule and measure holds; `points` are the new coordinates
    conflict, ///< rules contradict each other: `conflicting` names them, `points` are unchanged
    failed    ///< bad input or no solution found: `points` are unchanged, `detail` says why
};

struct SolveResult {
    SolveStatus status = SolveStatus::failed;
    /// The points in the order they were given; unchanged unless the status is `ok`.
    std::vector<SolverPoint> points;
    /// How many directions the sketch can still move in (0 = fully fixed); -1 when not analysed.
    int dof = -1;
    /// Ids of rules and measures that contradict others (status `conflict`).
    std::vector<std::string> conflicting;
    /// Ids of rules and measures that follow from the others already (also with status `ok`).
    std::vector<std::string> redundant;
    /// For developers: why a solve failed. Never shown to the person.
    std::string detail;
};

/// Solves sketches. One call does not change the solver: it may be used from several threads.
/// The same system always gives the same coordinates, bit for bit (rules and measures are added in
/// id order, there are no random start values).
class SketchSolver {
public:
    SketchSolver() = default;
    SketchSolver(const SketchSolver&) = delete;
    SketchSolver& operator=(const SketchSolver&) = delete;
    virtual ~SketchSolver() = default;

    /// Never throws: whatever goes wrong inside becomes `SolveStatus::failed`.
    virtual SolveResult solve(const SketchSystem& system, const SolveOptions& options) const = 0;
};

} // namespace fission::geo
