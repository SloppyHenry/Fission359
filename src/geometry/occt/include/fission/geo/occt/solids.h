// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fission/geo/mesh.h"
#include "fission/geo/solid.h"
#include "fission/geo/types.h"

namespace fission::geo::occt {

/// Box with the given edge lengths (mm); `corner` is the corner with the smallest coordinates.
/// Throws GeometryError for sizes that are not positive finite numbers or an invalid result.
Solid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner = {});

/// Cylinder standing on the plane z = base.z, axis along +Z, `base` is the centre of its bottom.
Solid makeCylinder(double radius, double height, Point3d base = {});

/// Volume, area, bounding box, topology counts and validity of a body.
SolidReport measure(const Solid& solid);

/// Tessellation for display. Throws GeometryError if the kernel cannot mesh the body.
TriangleMesh tessellate(const Solid& solid, const TessellationParams& params = {});

} // namespace fission::geo::occt
