// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Turning a sketch step into the solver's plain data and writing the solved coordinates back
// (AP-2.2b, ADR-0013 addendum 2.2a). The solver itself lives behind geo::SketchSolver (PlaneGCS in
// `src/geometry/sketch`); this file only maps between the document's SketchData and
// geo::SketchSystem: no Qt, no kernel.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/geo/sketch_solver.h"

namespace fission::core {

/// The values of the measures, by measure id (mm, or degrees for angles).
using SketchMeasureValues = std::map<std::string, double>;

/// The sketch as the solver sees it. Point ids are "<element id>.<index>" ("L1.0" = start of L1).
/// Curves: lines, circles and arcs (id = element id); splines and stand-alone points only give
/// points. A reference in a rule or measure that is a bare element id of a POINT element ("P1")
/// is mapped to its point "P1.0"; other bare ids stay curve ids; "L1.0" stays a point id.
/// Rules with an unknown kind word and measures without a value in `values` are left out.
/// Rules and measures are added sorted by their number ("R2" before "R10").
geo::SketchSystem toSolverSystem(const SketchData& sketch, const SketchMeasureValues& values);

/// Writes solved coordinates back into the elements (only when the status is ok). Elements with a
/// `link` (they follow a body edge) keep their points. Returns true when a coordinate changed.
bool applySolution(SketchData& sketch, const geo::SolveResult& result);

/// The values stored in the measures (`lastValue`).
SketchMeasureValues storedMeasureValues(const SketchData& sketch);

/// Builds, solves (default options), applies when ok. Returns the solver's result.
geo::SolveResult solveSketch(SketchData& sketch, const SketchMeasureValues& values,
                             const geo::SketchSolver& solver);

/// A live drag of sketch points (AP-2.2c4): analysed once when it starts, every move is fast.
/// Held points of elements with a `link` and unknown point ids are left out.
class SketchDragging {
public:
    /// `held`: solver point ids ("L1.1"), dragged together. The targets start where they are.
    SketchDragging(const SketchData& sketch, const SketchMeasureValues& values,
                   const geo::SketchSolver& solver, const std::vector<std::string>& held);
    /// False when nothing can be dragged (no usable held point, or the solver refused).
    bool started() const noexcept { return m_drag != nullptr; }
    /// The ids of the held points that are used, in the order `moveTo` expects its targets.
    const std::vector<std::string>& held() const noexcept { return m_held; }
    /// The sketch with the held points pulled to `targets` as far as the rules allow; nullopt when
    /// the solve fails (the caller keeps the last good state).
    std::optional<SketchData> moveTo(const std::vector<geo::Point2d>& targets);

private:
    SketchData m_start;
    std::vector<std::string> m_held;
    std::unique_ptr<geo::SketchDrag> m_drag;
};

/// The elements of the sketch that can still move after a solve (ids like "L1", "C2"): a line or
/// point when one of its points can move, a circle when its centre or radius can, an arc when its
/// centre, an end or its radius can. A point on the rim of a circle that only turns round it does
/// not count. Elements that are linked to an edge are fixed. Sorted.
std::vector<std::string> movableElements(const SketchData& sketch, const geo::SolveResult& result);

/// A rule the drawing fulfils exactly (AP-2.2c2).
struct AutoRule {
    std::string kind;
    std::vector<std::string> elements;
    bool operator==(const AutoRule&) const = default;
};

/// Rules the new elements (`newIds`, already part of `sketch`) fulfil exactly (within 1e-9 mm), to
/// be added when they are drawn: "horizontal"/"vertical" {line} for a level/upright line;
/// "coincident" {"<new>.<i>", "<other>.<j>"} for an end point on an end point of another element
/// (between two new ones each pair once); "tangent" {arc, line} for an arc that starts or ends at
/// an end of a line and runs on along it. Elements with a `link` and circles take no part. Rules
/// the sketch already has (same kind, same elements in any order) are left out.
std::vector<AutoRule> autoSketchRules(const SketchData& sketch,
                                      const std::vector<std::string>& newIds);

} // namespace fission::core
