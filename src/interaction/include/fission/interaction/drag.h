// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What a drag of the pointer means in the scene: how far along an axis, or where in a plane the
// pointer has gone since it grabbed something. The pointer is a ray from the eye; it is laid on a
// plane through what was grabbed, and the place where it meets that plane is followed. Pure
// geometry (mm), no window: the controllers of the interface use it, and the tests check it.

#pragma once

#include <QVector3D>

#include <optional>

namespace fission::interaction {

/// The pointer as a ray into the scene (mm). `direction` has unit length.
struct Ray {
    QVector3D origin;
    QVector3D direction;
};

/// A handle whose axis is closer than this (sine of the angle, 0.5 = 30 degrees) to the line of
/// sight cannot be dragged: the pointer would have to go ever so far for a little change.
inline constexpr double kLeastSight = 0.5;

/// A plane seen more edge on than this (sine of the angle between plane and line of sight) is not
/// dragged in: a small move of the pointer would throw the body far away.
inline constexpr double kLeastPlaneSight = 0.2;

/// Dragging along one axis (a size handle, an arrow of the gizmo). The pointer is laid on the
/// plane that holds the axis and faces the viewer most.
class AxisDrag {
public:
    /// Grabs at `origin` (a point on the axis). Nothing when the axis points (nearly) at the
    /// viewer or the ray does not meet the plane in front of the eye.
    static std::optional<AxisDrag> grab(const Ray& ray, const QVector3D& origin,
                                        const QVector3D& axis, double leastSight = kLeastSight);

    /// How far along the axis the pointer has gone since it grabbed (mm); nothing when the ray
    /// does not meet the plane in front of the eye.
    std::optional<double> moved(const Ray& ray) const;

    const QVector3D& axis() const noexcept { return axis_; }

private:
    QVector3D origin_;
    QVector3D axis_;
    QVector3D plane_;      // normal of the plane the pointer is laid on
    double grabbed_ = 0.0; // where along the axis the pointer was when it grabbed
};

/// Dragging in a plane (a plane square of the gizmo, a body pulled across the bed).
class PlaneDrag {
public:
    /// Grabs the point where the ray meets the plane through `origin` with normal `normal`.
    /// Nothing when the plane is seen (nearly) edge on or lies behind the eye.
    static std::optional<PlaneDrag> grab(const Ray& ray, const QVector3D& origin,
                                         const QVector3D& normal,
                                         double leastSight = kLeastPlaneSight);

    /// How far the pointer has gone in the plane since it grabbed (mm, a vector in the plane);
    /// nothing when the ray does not meet the plane in front of the eye.
    std::optional<QVector3D> moved(const Ray& ray) const;

    const QVector3D& normal() const noexcept { return normal_; }

private:
    QVector3D normal_;
    QVector3D grabbed_; // where the pointer met the plane when it grabbed
};

/// The step a dragged length snaps to: whole millimetres, finer when zoomed in, coarser only when
/// a millimetre is hardly a pixel on screen.
double snapStep(double pixelsPerMillimetre);

/// `value` on the nearest multiple of `step` (`step` > 0).
double snapped(double value, double step);

} // namespace fission::interaction
