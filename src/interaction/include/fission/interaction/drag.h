// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What a drag of the pointer means in the scene: how far along an axis, or where in a plane the
// pointer has gone since it grabbed something. The pointer is a ray from the eye; it is laid on a
// plane through what was grabbed, and the place where it meets that plane is followed. Pure
// geometry (mm), no window: the controllers of the interface use it, and the tests check it.

#pragma once

#include <QPointF>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <optional>

namespace fission::interaction {

/// The pointer as a ray into the scene (mm). `direction` has unit length.
struct Ray {
    QVector3D origin;
    QVector3D direction;
};

/// Above this (sine of the angle between axis and line of sight, 0.5 = 30 degrees) the pointer is
/// laid on a plane through the axis: the grabbed point stays exactly under the pointer. An axis
/// that points more at the viewer is dragged on the screen instead (`AxisDrag::Screen`).
inline constexpr double kLeastSight = 0.5;

/// A plane seen more edge on than this (sine of the angle between plane and line of sight) is not
/// dragged in: a small move of the pointer would throw the body far away.
inline constexpr double kLeastPlaneSight = 0.2;

/// Dragging along one axis (a size handle, an arrow of the gizmo). Every axis can be dragged:
/// - seen from the side, the pointer is laid on the plane that holds the axis and faces the
///   viewer most, and the grabbed point stays under the pointer;
/// - pointing (nearly) at the viewer, the pointer moving along the way the axis runs on screen
///   (up, when it points straight at the viewer) changes it, at a steady rate.
/// Near the vanishing point of the axis the plane would throw it ever further: a move that would
/// take it beyond `kFarthestReach` times the distance it was grabbed at is not followed.
class AxisDrag {
public:
    /// The axis as the screen shows it where it was grabbed.
    struct Screen {
        QPointF pointer;            ///< where the pointer went down (view pixels)
        QPointF way;                ///< unit vector: the way the axis runs on screen
        double pixelsPerMillimetre; ///< pointer pixels for one millimetre along the axis
    };

    /// How the screen shows `axis` at `origin`: `project` maps a world point to view pixels,
    /// `eye` is where the viewer is. A foreshortened axis takes at least `kLeastScreenShare` of
    /// the pixels per millimetre of an axis across the line of sight (it must not race away).
    /// One pointing straight at the viewer runs as seen from a little higher up: a level axis
    /// comes nearer down the screen, the upright one rises up the screen.
    template <typename Project>
    static Screen screenOf(const QPointF& pointer, const QVector3D& origin, const QVector3D& axis,
                           const QVector3D& eye, Project project);

    /// Grabs at `origin` (a point on the axis). Nothing only when the pointer does not meet the
    /// scene in front of the eye.
    static std::optional<AxisDrag> grab(const Ray& ray, const QVector3D& origin,
                                        const QVector3D& axis, const Screen& screen,
                                        double leastSight = kLeastSight);

    /// How far along the axis the pointer has gone since it grabbed (mm); nothing when this move
    /// is not followed (behind the eye, beyond the vanishing point).
    std::optional<double> moved(const Ray& ray, const QPointF& pointer) const;

    const QVector3D& axis() const noexcept { return axis_; }
    /// True when the axis is dragged on the screen (it points at the viewer).
    bool onScreen() const noexcept { return onScreen_; }

    static constexpr double kFarthestReach = 4.0;
    static constexpr double kLeastScreenShare = 0.75;

private:
    QVector3D origin_;
    QVector3D axis_;
    QVector3D plane_;      // normal of the plane the pointer is laid on
    double grabbed_ = 0.0; // where along the axis the pointer was when it grabbed
    double reach_ = 0.0;   // how far from the eye it was grabbed
    bool onScreen_ = false;
    Screen screen_{};
};

template <typename Project>
AxisDrag::Screen AxisDrag::screenOf(const QPointF& pointer, const QVector3D& origin,
                                    const QVector3D& axis, const QVector3D& eye, Project project) {
    const QVector3D unit = axis.normalized();
    QVector3D sight = origin - eye;
    sight = sight.lengthSquared() > 1e-12F ? sight.normalized() : QVector3D(0, 0, -1);
    // One millimetre across the line of sight, for the rate of a foreshortened axis.
    QVector3D across = QVector3D::crossProduct(sight, unit);
    if (across.lengthSquared() < 1e-8F) {
        across = QVector3D::crossProduct(sight, QVector3D(1, 0, 0));
        if (across.lengthSquared() < 1e-8F) {
            across = QVector3D::crossProduct(sight, QVector3D(0, 1, 0));
        }
    }
    across.normalize();
    const QPointF here = project(origin);
    const QPointF along = project(origin + unit) - here;
    const QPointF side = project(origin + across) - here;
    const double alongLength = std::hypot(along.x(), along.y());
    const double sideLength = std::hypot(side.x(), side.y());
    Screen screen;
    screen.pointer = pointer;
    screen.pixelsPerMillimetre = std::max(alongLength, kLeastScreenShare * sideLength);
    if (alongLength > 0.1 * sideLength && alongLength > 1e-9) {
        screen.way = along / alongLength;
    } else {
        // Straight at the viewer, the way is taken as it would be seen from a little higher up,
        // so that it does not flip when the view turns a little: a level axis comes nearer
        // going down the screen (as the bed does), the upright axis rises going up.
        const bool towards = QVector3D::dotProduct(unit, sight) < 0.0F;
        const bool fromAbove = sight.z() <= 0.0F; // level counts as above (the bed is below)
        const bool up =
            std::abs(unit.z()) >= 0.7F ? (unit.z() > 0.0F) == fromAbove : towards != fromAbove;
        screen.way = up ? QPointF(0.0, -1.0) : QPointF(0.0, 1.0);
    }
    if (screen.pixelsPerMillimetre < 1e-9) {
        screen.pixelsPerMillimetre = 1.0;
    }
    return screen;
}

/// Dragging in a plane (a plane square of the gizmo, a body pulled across the bed).
class PlaneDrag {
public:
    /// Grabs the point where the ray meets the plane through `origin` with normal `normal`.
    /// Nothing when the plane is seen (nearly) edge on or lies behind the eye.
    static std::optional<PlaneDrag> grab(const Ray& ray, const QVector3D& origin,
                                         const QVector3D& normal,
                                         double leastSight = kLeastPlaneSight);

    /// How far the pointer has gone in the plane since it grabbed (mm, a vector in the plane);
    /// nothing when the ray does not meet the plane in front of the eye, or only so far away
    /// (towards the horizon) that the body would be thrown out of sight.
    std::optional<QVector3D> moved(const Ray& ray) const;

    const QVector3D& normal() const noexcept { return normal_; }

    static constexpr double kFarthestReach = 4.0;

private:
    QVector3D normal_;
    QVector3D grabbed_;  // where the pointer met the plane when it grabbed
    double reach_ = 0.0; // how far from the eye that was
};

/// The step a dragged length snaps to: whole millimetres, finer when zoomed in, coarser only when
/// a millimetre is hardly a pixel on screen.
double snapStep(double pixelsPerMillimetre);

/// `value` on the nearest multiple of `step` (`step` > 0).
double snapped(double value, double step);

} // namespace fission::interaction
