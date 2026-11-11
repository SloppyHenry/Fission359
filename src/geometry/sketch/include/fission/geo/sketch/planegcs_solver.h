// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "fission/geo/sketch_solver.h"

namespace fission::geo::sketch {

/// The sketch solver built on PlaneGCS (DogLeg, with Levenberg-Marquardt as the fallback).
std::unique_ptr<SketchSolver> makePlaneGcsSolver();

} // namespace fission::geo::sketch
