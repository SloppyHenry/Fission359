// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/datum.h"

#include <cmath>
#include <initializer_list>
#include <numbers>

#include "fission/geo/error.h"

namespace fission::geo {

namespace {

bool finite(Point3d v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Point3d add(Point3d a, Point3d b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Point3d sub(Point3d a, Point3d b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Point3d scaled(Point3d v, double s) {
    return {v.x * s, v.y * s, v.z * s};
}

double dot(Point3d a, Point3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Point3d cross(Point3d a, Point3d b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

double length(Point3d v) {
    return std::sqrt(dot(v, v));
}

[[noreturn]] void throwInvalid(const char* detail, const char* subject) {
    throw GeometryError(GeometryErrorCode::invalidInput, detail, subject);
}

void requireFinite(std::initializer_list<Point3d> points, double value) {
    for (Point3d p : points) {
        if (!finite(p)) {
            throwInvalid("datum: a point is not a finite number", "datumDegenerate");
        }
    }
    if (!std::isfinite(value)) {
        throwInvalid("datum: a value is not a finite number", "datumDegenerate");
    }
}

} // namespace

DatumPlane offsetPlane(const DatumPlane& from, double distance) {
    requireFinite({from.origin, from.normal, from.uAxis}, distance);
    return {add(from.origin, scaled(from.normal, distance)), from.normal, from.uAxis};
}

DatumPlane planeThroughPoints(Point3d a, Point3d b, Point3d c) {
    requireFinite({a, b, c}, 0.0);
    const Point3d u = sub(b, a);
    const Point3d area = cross(u, sub(c, a)); // parallelogram area as a vector
    const double areaLength = length(area);
    if (areaLength < 1e-9) {
        throwInvalid("datum: three points on one line give no plane", "datumDegenerate");
    }
    return {a, scaled(area, 1.0 / areaLength), scaled(u, 1.0 / length(u))};
}

DatumPlane midPlane(const DatumPlane& a, const DatumPlane& b) {
    requireFinite({a.origin, a.normal, a.uAxis, b.origin, b.normal, b.uAxis}, 0.0);
    if (std::abs(dot(a.normal, b.normal)) < 1.0 - 1e-9) {
        throwInvalid("datum: the two planes are not parallel", "datumNotParallel");
    }
    // Signed distance of b's origin from a's plane along a's normal; half of it is the middle.
    const double half = 0.5 * dot(sub(b.origin, a.origin), a.normal);
    return {add(a.origin, scaled(a.normal, half)), a.normal, a.uAxis};
}

DatumPlane anglePlane(Point3d edgePoint, Point3d edgeDirection, const DatumPlane& reference,
                      double degrees) {
    requireFinite({edgePoint, edgeDirection, reference.origin, reference.normal, reference.uAxis},
                  degrees);
    const double edgeLength = length(edgeDirection);
    if (edgeLength < 1e-12) {
        throwInvalid("datum: the edge direction is zero", "datumDegenerate");
    }
    if (std::abs(dot(sub(edgePoint, reference.origin), reference.normal)) >= 1e-6 ||
        std::abs(dot(edgeDirection, reference.normal)) >= 1e-9) {
        throwInvalid("datum: the edge does not lie in the reference plane", "datumEdgeNotInPlane");
    }
    const Point3d axis = scaled(edgeDirection, 1.0 / edgeLength);
    const double radians = degrees * std::numbers::pi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    // Rodrigues: n' = n·cos + (axis × n)·sin + axis·(axis·n)·(1−cos).
    const Point3d n = reference.normal;
    const Point3d turned =
        add(add(scaled(n, c), scaled(cross(axis, n), s)), scaled(axis, dot(axis, n) * (1.0 - c)));
    const double turnedLength = length(turned);
    return {edgePoint, scaled(turned, 1.0 / turnedLength), axis};
}

DatumPlane tangentPlane(Point3d axisPoint, Point3d axisDirection, double radius,
                        Point3d radialDirection) {
    requireFinite({axisPoint, axisDirection, radialDirection}, radius);
    if (!(radius > 0.0)) {
        throwInvalid("datum: the radius is not positive", "datumDegenerate");
    }
    const double axisLength = length(axisDirection);
    if (axisLength < 1e-12) {
        throwInvalid("datum: the axis direction is zero", "datumDegenerate");
    }
    const Point3d axis = scaled(axisDirection, 1.0 / axisLength);
    // The radial direction projected into the plane perpendicular to the axis.
    const Point3d radial = sub(radialDirection, scaled(axis, dot(radialDirection, axis)));
    const double radialLength = length(radial);
    if (radialLength < 1e-12) {
        throwInvalid("datum: the radial direction is parallel to the axis", "datumDegenerate");
    }
    const Point3d normal = scaled(radial, 1.0 / radialLength);
    return {add(axisPoint, scaled(normal, radius)), normal, axis};
}

} // namespace fission::geo
