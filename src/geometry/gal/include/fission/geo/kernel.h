// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/mesh.h"
#include "fission/geo/solid.h"
#include "fission/geo/types.h"

namespace fission::geo {

/// A face together with the role its creator gave it ("top", "side"). `faceId` counts like
/// `TriangleMesh::faceIds` (1-based, in the order of the body's faces).
struct NamedFace {
    std::uint32_t faceId = 0;
    std::string role;
};

/// A body and the roles of its faces. The naming contract (ADR-0003): an operation names **every**
/// face it creates, so `faces` has exactly one entry per face of `solid`.
struct NamedSolid {
    Solid solid;
    std::vector<NamedFace> faces;
};

/// What features may ask of the geometry kernel (docs/03-architektur.md §4). The core and the
/// feature types only know this interface; the OpenCASCADE implementation is handed in by the
/// application, tests may hand in another one.
///
/// All functions are safe to call from several threads at once and throw `GeometryError` for
/// input the kernel cannot work with or results that fail validation.
class GeometryKernel {
public:
    GeometryKernel() = default;
    GeometryKernel(const GeometryKernel&) = delete;
    GeometryKernel& operator=(const GeometryKernel&) = delete;
    virtual ~GeometryKernel() = default;

    /// Box with the given edge lengths (mm); `corner` has the smallest coordinates.
    /// Faces: bottom, top, front (−Y), back (+Y), left (−X), right (+X).
    virtual NamedSolid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner) const = 0;

    /// Cylinder standing on `base` (centre of its bottom), axis +Z.
    /// Faces: bottom, top, side.
    virtual NamedSolid makeCylinder(double radius, double height, Point3d base) const = 0;

    virtual SolidReport measure(const Solid& solid) const = 0;

    virtual TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) const = 0;
};

} // namespace fission::geo
