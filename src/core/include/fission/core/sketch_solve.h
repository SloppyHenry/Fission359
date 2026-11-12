// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Turning a sketch step into the solver's plain data and writing the solved coordinates back
// (AP-2.2b, ADR-0013 addendum 2.2a). The solver itself lives behind geo::SketchSolver (PlaneGCS in
// `src/geometry/sketch`); this file only maps between the document's SketchData and
// geo::SketchSystem: no Qt, no kernel.

#pragma once

#include <map>
#include <string>

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

} // namespace fission::core
