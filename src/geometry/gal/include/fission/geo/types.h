// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace fission::geo {

/// A point or vector in model space. Unit: millimetres, double precision (docs/03-architektur.md
/// §6).
struct Point3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    bool operator==(const Point3d&) const = default;
};

/// A point or vector for display data (GPU-friendly single precision).
struct Point3f {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

} // namespace fission::geo
