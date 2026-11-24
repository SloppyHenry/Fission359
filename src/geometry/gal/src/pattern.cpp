// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/pattern.h"

#include <cmath>
#include <numbers>

namespace fission::geo {

namespace {

// Helper functions for Point3d operations

/// Compute the dot product of two 3D vectors
inline double dot(Point3d a, Point3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// Compute the length (magnitude) of a 3D vector
inline double length(Point3d v) {
    return std::sqrt(dot(v, v));
}

/// Normalize a 3D vector. Returns zero vector if input is zero.
inline Point3d normalize(Point3d v) {
    const double len = length(v);
    if (len < 1e-12) {
        return {0, 0, 0};
    }
    return {v.x / len, v.y / len, v.z / len};
}

/// Check if a vector is finite (all components are finite)
inline bool isFinite(Point3d v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/// Cross product of two 3D vectors

/// Scale a vector by a scalar
inline Point3d scale(Point3d v, double s) {
    return {v.x * s, v.y * s, v.z * s};
}

/// Add two vectors
inline Point3d add(Point3d a, Point3d b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

/// Subtract two vectors
inline Point3d subtract(Point3d a, Point3d b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

/// Matrix-vector multiplication: R * v where R is 3x3 row-major
inline Point3d matVecMul(const double r[3][3], Point3d v) {
    return {r[0][0] * v.x + r[0][1] * v.y + r[0][2] * v.z,
            r[1][0] * v.x + r[1][1] * v.y + r[1][2] * v.z,
            r[2][0] * v.x + r[2][1] * v.y + r[2][2] * v.z};
}

/// Compute Rodrigues' rotation matrix for rotation around axis by angle (in radians)
/// R = I + sin(theta) * K + (1 - cos(theta)) * K^2
/// where K is the cross-product matrix of the axis
inline void rodriguesRotation(double r[3][3], Point3d axis, double angleRad) {
    const double c = std::cos(angleRad);
    const double s = std::sin(angleRad);
    const double t = 1.0 - c;

    const double ax = axis.x, ay = axis.y, az = axis.z;
    const double ax2 = ax * ax, ay2 = ay * ay, az2 = az * az;
    const double axy = ax * ay, axz = ax * az, ayz = ay * az;

    r[0][0] = t * ax2 + c;
    r[0][1] = t * axy - s * az;
    r[0][2] = t * axz + s * ay;

    r[1][0] = t * axy + s * az;
    r[1][1] = t * ay2 + c;
    r[1][2] = t * ayz - s * ax;

    r[2][0] = t * axz - s * ay;
    r[2][1] = t * ayz + s * ax;
    r[2][2] = t * az2 + c;
}

/// Create identity matrix
inline void identityMatrix(double r[3][3]) {
    r[0][0] = 1;
    r[0][1] = 0;
    r[0][2] = 0;
    r[1][0] = 0;
    r[1][1] = 1;
    r[1][2] = 0;
    r[2][0] = 0;
    r[2][1] = 0;
    r[2][2] = 1;
}

/// Copy a 3x3 matrix

/// Check if all inputs are finite

} // namespace

// ---------------------------------------------------------------------------
// Transform3 operations
// ---------------------------------------------------------------------------

Point3d apply(const Transform3& transform, Point3d point) {
    const Point3d rotated = matVecMul(transform.r, point);
    return add(rotated, transform.t);
}

Point3d applyToDirection(const Transform3& transform, Point3d direction) {
    return matVecMul(transform.r, direction);
}

double determinant(const Transform3& transform) {
    const auto& r = transform.r;
    return r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) -
           r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0]) +
           r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);
}

// ---------------------------------------------------------------------------
// Pattern functions
// ---------------------------------------------------------------------------

std::vector<Transform3> linearPattern(Point3d direction, double spacing, int count) {
    // Validate inputs
    if (!isFinite(direction) || !std::isfinite(spacing)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Input must be finite",
                            "patternDegenerate");
    }
    if (count < 1 || count > 10000) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Count must be in [1, 10000]",
                            "patternCount");
    }

    const double dirLen = length(direction);
    if (dirLen < 1e-12) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Direction has zero length",
                            "patternDegenerate");
    }

    std::vector<Transform3> result;
    result.reserve(count);

    const Point3d unitDir = normalize(direction);

    for (int i = 0; i < count; ++i) {
        Transform3 t;
        identityMatrix(t.r);
        t.t = scale(unitDir, i * spacing);
        result.push_back(t);
    }

    return result;
}

std::vector<Transform3> linearPatternOverLength(Point3d direction, double totalLength, int count) {
    // Validate inputs
    if (!isFinite(direction) || !std::isfinite(totalLength)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Input must be finite",
                            "patternDegenerate");
    }
    if (count < 2) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Count must be at least 2",
                            "patternCount");
    }

    const double dirLen = length(direction);
    if (dirLen < 1e-12) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Direction has zero length",
                            "patternDegenerate");
    }

    const double spacing = totalLength / (count - 1);
    return linearPattern(direction, spacing, count);
}

std::vector<Transform3> gridPattern(Point3d directionA, double spacingA, int countA,
                                    Point3d directionB, double spacingB, int countB) {
    // Validate inputs
    if (!isFinite(directionA) || !isFinite(directionB) || !std::isfinite(spacingA) ||
        !std::isfinite(spacingB)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Input must be finite",
                            "patternDegenerate");
    }

    const long long totalCount = static_cast<long long>(countA) * countB;
    if (countA < 1 || countB < 1 || totalCount > 10000) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Total count must not exceed 10000",
                            "patternCount");
    }

    const double dirALen = length(directionA);
    const double dirBLen = length(directionB);
    if (dirALen < 1e-12 || dirBLen < 1e-12) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Direction has zero length",
                            "patternDegenerate");
    }

    std::vector<Transform3> result;
    result.reserve(countA * countB);

    const Point3d unitA = normalize(directionA);
    const Point3d unitB = normalize(directionB);

    for (int j = 0; j < countB; ++j) {
        for (int i = 0; i < countA; ++i) {
            Transform3 t;
            identityMatrix(t.r);
            t.t = add(scale(unitA, i * spacingA), scale(unitB, j * spacingB));
            result.push_back(t);
        }
    }

    return result;
}

std::vector<Transform3> circularPattern(Point3d axisPoint, Point3d axisDirection, int count,
                                        double totalDegrees) {
    // Validate inputs
    if (!isFinite(axisPoint) || !isFinite(axisDirection) || !std::isfinite(totalDegrees)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Input must be finite",
                            "patternDegenerate");
    }
    if (count < 1 || count > 10000) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Count must be in [1, 10000]",
                            "patternCount");
    }

    const double axisLen = length(axisDirection);
    if (axisLen < 1e-12) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Axis direction has zero length",
                            "patternDegenerate");
    }

    // For totalDegrees != 360, we need count >= 2
    if (std::abs(totalDegrees - 360.0) > 1e-12 && count < 2) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "Count must be at least 2 for non-360 patterns", "patternCount");
    }

    std::vector<Transform3> result;
    result.reserve(count);

    const Point3d unitAxis = normalize(axisDirection);
    const double angleRadStep = totalDegrees == 360.0
                                    ? (2.0 * std::numbers::pi / count)
                                    : (totalDegrees * std::numbers::pi / 180.0 / (count - 1));

    // Pre-compute rotation around axis by angleRadStep
    for (int i = 0; i < count; ++i) {
        const double angleRad = i * angleRadStep;

        Transform3 t;
        rodriguesRotation(t.r, unitAxis, angleRad);

        // t = axisPoint - R * axisPoint
        // This ensures points on the axis remain fixed
        const Point3d rotatedAxisPoint = matVecMul(t.r, axisPoint);
        t.t = subtract(axisPoint, rotatedAxisPoint);

        result.push_back(t);
    }

    return result;
}

Transform3 mirrorPlane(Point3d planePoint, Point3d planeNormal) {
    // Validate inputs
    if (!isFinite(planePoint) || !isFinite(planeNormal)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Input must be finite",
                            "patternDegenerate");
    }

    const double normLen = length(planeNormal);
    if (normLen < 1e-12) {
        throw GeometryError(GeometryErrorCode::invalidInput, "Plane normal has zero length",
                            "patternDegenerate");
    }

    Transform3 t;

    // Normalize the normal vector
    const Point3d n = normalize(planeNormal);

    // R = I - 2 * n * n^T (outer product)
    // This is a symmetric matrix: R_ij = delta_ij - 2 * n_i * n_j
    t.r[0][0] = 1.0 - 2.0 * n.x * n.x;
    t.r[0][1] = -2.0 * n.x * n.y;
    t.r[0][2] = -2.0 * n.x * n.z;

    t.r[1][0] = -2.0 * n.y * n.x;
    t.r[1][1] = 1.0 - 2.0 * n.y * n.y;
    t.r[1][2] = -2.0 * n.y * n.z;

    t.r[2][0] = -2.0 * n.z * n.x;
    t.r[2][1] = -2.0 * n.z * n.y;
    t.r[2][2] = 1.0 - 2.0 * n.z * n.z;

    // t = 2 * (n · planePoint) * n
    const double dotProduct = dot(n, planePoint);
    t.t = scale(n, 2.0 * dotProduct);

    return t;
}

std::vector<Transform3> withSkipped(const std::vector<Transform3>& all,
                                    const std::vector<int>& skipIndices) {
    std::vector<Transform3> result;
    result.reserve(all.size());

    // First, collect all indices to skip (except 0)
    std::vector<bool> skip(all.size(), false);
    for (int idx : skipIndices) {
        if (idx > 0 && idx < static_cast<int>(all.size())) {
            skip[idx] = true;
        }
    }

    // Copy all non-skipped transformations
    for (size_t i = 0; i < all.size(); ++i) {
        if (!skip[i]) {
            result.push_back(all[i]);
        }
    }

    return result;
}

} // namespace fission::geo
