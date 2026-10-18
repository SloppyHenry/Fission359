// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/mesh.h"
#include "fission/geo/primitives.h"
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
    /// `cornerRadius` rounds the four vertical edges (0 = sharp, at most half the shorter side).
    /// Faces: bottom, top, front (−Y), back (+Y), left (−X), right (+X), and with rounded
    /// corners cornerFrontLeft, cornerFrontRight, cornerBackLeft, cornerBackRight.
    virtual NamedSolid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner,
                               double cornerRadius) const = 0;

    /// Cylinder standing on `base` (centre of its bottom), axis +Z.
    /// Faces: bottom, top, side.
    virtual NamedSolid makeCylinder(double radius, double height, Point3d base) const = 0;

    /// One of the other basic shapes (see primitives.h); `base` is the middle of its underside.
    virtual NamedSolid makePrimitive(const PrimitiveSpec& spec, Point3d base) const = 0;

    /// The names of the fonts lettering can use (bundled and installed), sorted. Takes a moment
    /// the first time (the computer's fonts are read): ask from a background thread.
    virtual std::vector<std::string> fontFamilies() const { return {}; }

    /// The styles a font has (always in the order regular, bold, italic, boldItalic); empty if the
    /// font does not exist.
    virtual std::vector<FontStyle> fontStyles(const std::string& family) const {
        (void)family;
        return {};
    }

    virtual SolidReport measure(const Solid& solid) const = 0;

    /// The box round a body (mm), quickly: no volume, no validity check. For deciding how finely
    /// a big body is drawn. The default asks `measure`.
    virtual Bounds bounds(const Solid& solid) const {
        const SolidReport report = measure(solid);
        return {report.boxMin, report.boxMax};
    }

    virtual TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) const = 0;
};

} // namespace fission::geo
