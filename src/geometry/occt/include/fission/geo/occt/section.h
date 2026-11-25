// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fission/geo/section.h"
#include "fission/geo/solid.h"

namespace fission::geo::occt {

/// See `GeometryKernel::sectionMesh`.
SectionMesh sectionMesh(const Solid& solid, Point3d planePoint, Point3d planeNormal,
                        double deflection);

} // namespace fission::geo::occt
