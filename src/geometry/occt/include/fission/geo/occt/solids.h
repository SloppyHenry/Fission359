// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fission/geo/kernel.h"
#include "fission/geo/mesh.h"
#include "fission/geo/primitives.h"
#include "fission/geo/solid.h"
#include "fission/geo/types.h"

namespace fission::geo::occt {

/// Box with the given edge lengths (mm); `corner` is the corner with the smallest coordinates.
/// Throws GeometryError for sizes that are not positive finite numbers or an invalid result.
/// `cornerRadius` rounds the four vertical edges (0 = sharp).
Solid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner = {},
              double cornerRadius = 0.0);

/// Cylinder standing on the plane z = base.z, axis along +Z, `base` is the centre of its bottom.
Solid makeCylinder(double radius, double height, Point3d base = {});

/// Like `makeBox`, with the role of every face (see `GeometryKernel::makeBox`).
NamedSolid makeNamedBox(double sizeX, double sizeY, double sizeZ, Point3d corner = {},
                        double cornerRadius = 0.0);

/// The basic shapes of `fission/geo/primitives.h`, with the role of every face.
/// Throws GeometryError for sizes that are not usable or an invalid result.
NamedSolid makeNamedPrimitive(const PrimitiveSpec& spec, Point3d base = {});

/// Like `makeCylinder`, with the role of every face (bottom, top, side).
NamedSolid makeNamedCylinder(double radius, double height, Point3d base = {});

/// Volume, area, bounding box, topology counts and validity of a body.
SolidReport measure(const Solid& solid);

/// Tessellation for display. Throws GeometryError if the kernel cannot mesh the body.
TriangleMesh tessellate(const Solid& solid, const TessellationParams& params = {});

} // namespace fission::geo::occt
