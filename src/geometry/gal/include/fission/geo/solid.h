// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cmath>
#include <memory>
#include <utility>

#include "fission/geo/types.h"

namespace fission::geo {

/// Kernel-specific data of a body. Only the kernel implementation that created a Solid
/// knows the concrete type; every other layer treats it as opaque.
class SolidData {
public:
    SolidData() = default;
    SolidData(const SolidData&) = delete;
    SolidData& operator=(const SolidData&) = delete;
    virtual ~SolidData() = default;
};

/// A precise body (B-Rep). Cheap to copy: copies share the immutable kernel data.
class Solid {
public:
    Solid() = default;
    explicit Solid(std::shared_ptr<const SolidData> data) : data_(std::move(data)) {}

    bool empty() const noexcept { return data_ == nullptr; }
    const SolidData* data() const noexcept { return data_.get(); }

private:
    std::shared_ptr<const SolidData> data_;
};

/// What golden tests and the property panel need to know about a body.
struct SolidReport {
    double volume = 0.0; ///< mm³
    double area = 0.0;   ///< mm²
    Point3d boxMin;      ///< Bounding box corner with the smallest coordinates.
    Point3d boxMax;      ///< Bounding box corner with the largest coordinates.
    int solids = 0;
    int faces = 0;
    int edges = 0;
    int vertices = 0;
    bool valid = false; ///< Passed the kernel's validity check.
};

/// A box along the axes, from its smallest to its largest corner (mm).
struct Bounds {
    Point3d min;
    Point3d max;
    /// The length of its diagonal (0 for an empty box).
    double diagonal() const { return std::hypot(max.x - min.x, max.y - min.y, max.z - min.z); }
};

/// How finely a body is tessellated for display or export.
struct TessellationParams {
    double chordTolerance = 0.01;         ///< Largest distance between mesh and true surface in mm.
    double angleTolerance = 0.0872664626; ///< Largest angle between neighbouring normals (5°).
};

} // namespace fission::geo
