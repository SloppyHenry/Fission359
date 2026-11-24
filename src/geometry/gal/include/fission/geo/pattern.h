// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/types.h"

namespace fission::geo {

/// A 3D transformation: p' = R * p + t, where R is a 3x3 rotation matrix and t is a translation
/// vector. A reflection has determinant -1.
struct Transform3 {
    double r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}; ///< Rotation matrix (row-major)
    Point3d t = {0, 0, 0};                              ///< Translation vector
};

/// Apply transformation to a point: p' = R * p + t
Point3d apply(const Transform3& transform, Point3d point);

/// Apply transformation to a direction vector (translation ignored): v' = R * v
Point3d applyToDirection(const Transform3& transform, Point3d direction);

/// Compute the determinant of the rotation matrix R
double determinant(const Transform3& transform);

/// Create a linear pattern of transformations along a direction.
/// Each instance i is a translation by i * spacing along the normalized direction.
/// Instance 0 is the identity (original).
///
/// Throws GeometryError with code invalidInput and subject:
/// - "patternCount" if count is not in [1, 10000]
/// - "patternDegenerate" if direction has length < 1e-12
std::vector<Transform3> linearPattern(Point3d direction, double spacing, int count);

/// Create a linear pattern where the total length is distributed evenly.
/// The spacing between instances is totalLength / (count - 1).
/// Instance 0 is the identity (original).
///
/// Throws GeometryError with code invalidInput and subject:
/// - "patternCount" if count < 2
/// - "patternDegenerate" if direction has length < 1e-12
std::vector<Transform3> linearPatternOverLength(Point3d direction, double totalLength, int count);

/// Create a 2D grid pattern with two directions.
/// Indexing: index = j * countA + i, where i runs along directionA and j along directionB.
/// Instance 0 is the identity (original at i=0, j=0).
/// Total number of instances is countA * countB, which must not exceed 10000.
///
/// Throws GeometryError with code invalidInput and subject:
/// - "patternCount" if countA * countB > 10000
/// - "patternDegenerate" if either direction has length < 1e-12
std::vector<Transform3> gridPattern(Point3d directionA, double spacingA, int countA,
                                    Point3d directionB, double spacingB, int countB);

/// Create a circular pattern by rotating around an axis.
/// The rotation is right-handed using Rodrigues' rotation formula.
/// Instance 0 is the identity (original).
///
/// For totalDegrees == 360, the angular step is 360 / count (last instance is not duplicated).
/// For totalDegrees != 360, the angular step is totalDegrees / (count - 1) and count >= 2.
///
/// The translation t is set so that points on the axis remain fixed: t = axisPoint - R * axisPoint.
///
/// Throws GeometryError with code invalidInput and subject:
/// - "patternCount" if count < 1, count > 10000, or (totalDegrees != 360 and count < 2)
/// - "patternDegenerate" if axisDirection has length < 1e-12
std::vector<Transform3> circularPattern(Point3d axisPoint, Point3d axisDirection, int count,
                                        double totalDegrees);

/// Create a mirror transformation for a plane.
/// The plane is defined by a point and a normal vector.
/// R = I - 2 * n * n^T where n is the normalized normal.
/// t = 2 * (n · planePoint) * n.
///
/// Throws GeometryError with code invalidInput and subject:
/// - "patternDegenerate" if planeNormal has length < 1e-12
Transform3 mirrorPlane(Point3d planePoint, Point3d planeNormal);

/// Create a new vector of transformations with specified indices skipped.
/// Index 0 (the identity/original) cannot be skipped and is ignored if requested.
/// Indices outside the range are silently ignored.
/// The order of the remaining transformations is preserved.
std::vector<Transform3> withSkipped(const std::vector<Transform3>& all,
                                    const std::vector<int>& skipIndices);

} // namespace fission::geo
