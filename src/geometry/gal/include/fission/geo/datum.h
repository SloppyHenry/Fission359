// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Datum planes (AP-2.12): pure maths on Point3d, no kernel, no OCCT. The same frame as a
// SketchPlane: unit normal and unit u axis, perpendicular to each other; v = normal × u.

#pragma once

#include "fission/geo/types.h"

namespace fission::geo {

/// A plane a datum feature is built on: `origin` is a point of the plane, `normal` and `uAxis`
/// are unit vectors and perpendicular (v = normal × u, as in SketchPlane).
struct DatumPlane {
    Point3d origin;
    Point3d normal;
    Point3d uAxis;

    bool operator==(const DatumPlane&) const = default;
};

/// The plane moved `distance` mm along its normal (negative goes backwards, 0 keeps it).
DatumPlane offsetPlane(const DatumPlane& from, double distance);

/// The plane through three points: origin `a`, u axis along a→b, normal (b−a)×(c−a) unit.
/// Points on one line (parallelogram area < 1e-9) or not finite: invalidInput, "datumDegenerate".
DatumPlane planeThroughPoints(Point3d a, Point3d b, Point3d c);

/// The plane halfway between two parallel planes (|n_a·n_b| ≥ 1−1e-9, same or opposite normal):
/// the midpoint of the origins along a's normal, normal and u axis of `a`. Not parallel:
/// invalidInput, "datumNotParallel". Coincident planes give that plane.
DatumPlane midPlane(const DatumPlane& a, const DatumPlane& b);

/// The plane through the edge (edgePoint, edgeDirection) turned `degrees` against `reference`
/// about that edge (positive right-handed about edgeDirection, Rodrigues). The edge must lie in
/// the reference plane: invalidInput, "datumEdgeNotInPlane". Zero edge direction:
/// "datumDegenerate".
DatumPlane anglePlane(Point3d edgePoint, Point3d edgeDirection, const DatumPlane& reference,
                      double degrees);

/// The plane tangent to a cylinder (axis through axisPoint along axisDirection) at the line in
/// `radialDirection`: the radial direction is projected perpendicular to the axis and normalised;
/// origin = axisPoint + normal·radius. Radius ≤ 0, zero axis or radial parallel to the axis:
/// invalidInput, "datumDegenerate".
DatumPlane tangentPlane(Point3d axisPoint, Point3d axisDirection, double radius,
                        Point3d radialDirection);

} // namespace fission::geo
