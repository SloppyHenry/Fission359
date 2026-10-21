// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/kernel.h"
#include "fission/geo/mesh.h"
#include "fission/geo/occt/fonts.h"
#include "fission/geo/primitives.h"
#include "fission/geo/rotation.h"
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
NamedSolid makeNamedPrimitive(const PrimitiveSpec& spec, Point3d base = {},
                              const FontBook& fonts = {});

/// Lettering with the fonts of `fonts` (see `TextSpec`).
NamedSolid makeNamedText(const TextSpec& spec, Point3d base, const FontBook& fonts);

/// Like `makeCylinder`, with the role of every face (bottom, top, side).
NamedSolid makeNamedCylinder(double radius, double height, Point3d base = {});

/// The body turned by `angles` around `centre` (see `GeometryKernel::rotate`): same faces in the
/// same order with the same roles. Throws GeometryError for values that are not numbers.
NamedSolid rotateNamed(const NamedSolid& solid, Point3d centre, const Angles& angles);

/// The body moved by `delta` (see `GeometryKernel::translate`).
NamedSolid translateNamed(const NamedSolid& solid, Point3d delta);

/// Combines bodies (see `GeometryKernel::boolean`). Throws GeometryError.
NamedSolid booleanNamed(BooleanOp op, const NamedSolid& target,
                        const std::vector<NamedSolid>& tools);

/// Volume, area, bounding box, topology counts and validity of a body.
SolidReport measure(const Solid& solid);

/// The box round a body, from its geometry (no mesh needed).
Bounds bounds(const Solid& solid);
/// The box that touches the body (see `GeometryKernel::exactBounds`).
Bounds exactBounds(const Solid& solid);

/// Tessellation for display. Throws GeometryError if the kernel cannot mesh the body.
TriangleMesh tessellate(const Solid& solid, const TessellationParams& params = {});

} // namespace fission::geo::occt
