// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Offset, rounding and chamfering corners in the sketch mode (AP-2.1e2): pure, exact maths in plane
// coordinates (mm) for lines, arcs and circles, no window and no document. What comes out is a
// `SketchEdit` (see sketch_edit.h) that the sketch controller runs as one transaction. Curves
// (splines) are never offset or cornered: that gives a notice and nothing is made (rule 6).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/geo/kernel.h"
#include "fission/interaction/sketch_edit.h"

namespace fission::interaction {

// --- chains and offset --------------------------------------------------------------------

/// One element of a chain, in the direction of travel along the chain.
struct SketchChainSegment {
    std::string element;
    bool arc = false;      ///< an arc or a whole circle (else a line)
    bool circle = false;   ///< a whole circle (start and end are the same point)
    bool reversed = false; ///< travelled against the direction the element is stored in
    core::SketchPoint start;
    core::SketchPoint end;
    core::SketchPoint centre; ///< arc
    double radius = 0.0;      ///< arc
    bool ccw = true;          ///< arc: travelled counter-clockwise
    double span = 0.0;        ///< line: its length (mm); arc: the sweep in the direction of travel
};

/// Elements whose ends lie on each other (within 1e-6 mm), in order; a circle, or a ring of
/// elements, is closed. A chain stops where three or more ends meet.
struct SketchChain {
    std::vector<SketchChainSegment> segments;
    bool closed = false;
    bool construction = false;
    bool curve = false; ///< the element asked for is a curve (spline): it has no chain
};

/// The chain of the line, arc or circle with this id; nothing for a point or an unknown id.
/// A spline gives a chain with `curve` set and no segments. Only elements with the same
/// "helper line" mark are linked.
std::optional<SketchChain> findChain(const std::vector<core::SketchElement>& elements,
                                     const std::string& id);

/// The element (line, arc, circle, or curve: by its chords) nearest to `at` within `reach` mm.
std::optional<std::string> elementNear(const std::vector<core::SketchElement>& elements,
                                       const core::SketchPoint& at, double reach);

/// How far `at` is from the chain: positive when it is on the left of the direction of travel,
/// negative on the right (a circle runs counter-clockwise: positive = inside).
double chainSignedDistance(const SketchChain& chain, const core::SketchPoint& at);

enum class SketchOffsetNotice {
    none,
    curve,    ///< curves cannot be offset yet
    tooBig,   ///< the distance is bigger than a rounding of the chain (radius 0 or less)
    crossing, ///< the result would cross itself
    failed    ///< the corners could not be joined, or the kernel failed
};

struct SketchOffsetResult {
    SketchEdit edit; ///< new elements only (the original stays); empty with a notice
    SketchOffsetNotice notice = SketchOffsetNotice::none;
};

/// The chain moved sideways by `signedDistance` (positive = left of the direction of travel).
/// Lines move parallel and are joined at the corners (the crossing of the moved lines; round a
/// convex corner that touches an arc by an arc round the old corner; tangent transitions stay
/// smooth); arcs and circles change their radius only. A distance under 0.01 mm gives nothing.
/// The kernel is asked only to check that the result does not cross itself.
SketchOffsetResult offsetChain(const geo::GeometryKernel& kernel, const SketchChain& chain,
                               double signedDistance);

// --- corners --------------------------------------------------------------------------------

/// A corner: the point where the ends of exactly two elements meet (within 1e-6 mm).
struct SketchCorner {
    core::SketchPoint at;
    std::string first;
    std::string second;
    bool firstAtEnd = false; ///< the corner is at the end the first element is stored with
    bool secondAtEnd = false;
    bool curve = false; ///< one of them is a curve: no rounding
};

/// The corner nearest to `at` within `reach` mm; nothing when there is none near.
std::optional<SketchCorner> findCorner(const std::vector<core::SketchElement>& elements,
                                       const core::SketchPoint& at, double reach);

enum class SketchCornerNotice {
    none,
    curve,    ///< corners at curves are not rounded yet
    noCorner, ///< the two elements do not make a corner (they go on smoothly)
    tooSmall, ///< under 0.01 mm
    tooBig,   ///< no room: see `limit`
    failed
};

struct SketchCornerResult {
    SketchEdit edit; ///< moves the two ends and adds the arc (or line); empty with a notice
    SketchCornerNotice notice = SketchCornerNotice::none;
    double limit = 0.0; ///< tooBig: the biggest radius (distance) that fits, rounded down to 0.01
};

/// Rounds the corner with a tangent arc of this radius (mm).
SketchCornerResult roundCorner(const std::vector<core::SketchElement>& elements,
                               const SketchCorner& corner, double radius);
/// Cuts the corner with a straight line; `distance` (mm) is how far from the corner it starts on
/// each element (along the element).
SketchCornerResult chamferCorner(const std::vector<core::SketchElement>& elements,
                                 const SketchCorner& corner, double distance);

/// The radius / distance a pointer at `at` means while the corner is being pulled: the radius is
/// the distance of the pointer from the nearer of the two elements (the middle of the arc follows
/// the pointer), the distance is measured from the corner along the bisector. Zero when the
/// elements make no corner.
double cornerRadiusAt(const std::vector<core::SketchElement>& elements, const SketchCorner& corner,
                      const core::SketchPoint& at);
double cornerDistanceAt(const std::vector<core::SketchElement>& elements,
                        const SketchCorner& corner, const core::SketchPoint& at);

} // namespace fission::interaction
