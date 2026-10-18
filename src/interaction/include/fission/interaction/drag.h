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

/// Turning on a ring (AP-1.6b): the pointer goes round an axis through the middle of a body, and
/// the body follows by the same angle. Counter-clockwise seen from the tip of the axis is
/// positive.
/// - Seen from a good angle, the pointer is laid on the plane of the ring and the angle round
///   the middle is followed (the point grabbed stays under the pointer).
/// - A ring seen (nearly) edge on has no angle to follow in the plane: the pointer then runs along
///   the ring as it is seen on the screen, at the rate the ring has there.
/// The angle adds up: going round more than once goes on counting.
class RingDrag {
public:
    /// A ring seen more edge on than this (sine of the angle between its plane and the line of
    /// sight) is dragged on the screen.
    static constexpr double kLeastRingSight = 0.3;
    /// The fewest pixels the ring is taken to be wide (radius) on the screen; a tiny ring would
    /// otherwise turn the body wildly.
    static constexpr double kLeastScreenRadius = 30.0;

    /// Grabs the ring round `axis` (a unit vector along X, Y or Z, say) through `centre`, at the
    /// point of the pointer. `project` maps a world point to view pixels. `radius` is the size of
    /// the ring as drawn (millimetres); 0 takes the distance of the pointer from the middle.
    /// Nothing when the pointer does not meet the scene in front of the eye.
    template <typename Project>
    static std::optional<RingDrag> grab(const Ray& ray, const QPointF& pointer,
                                        const QVector3D& centre, const QVector3D& axis,
                                        Project project, double radius = 0.0);

    /// How far the body has been turned since the grab (degrees, not limited to one turn).
    /// Nothing when this move is not followed (behind the eye).
    std::optional<double> moved(const Ray& ray, const QPointF& pointer);

    const QVector3D& axis() const noexcept { return axis_; }
    bool onScreen() const noexcept { return onScreen_; }
    /// Where on the ring (radians from `u`, counter-clockwise) the pointer grabbed; the ring
    /// round `axis` is drawn from `ringPoint`.
    double grabbedAt() const noexcept { return first_; }
    /// The point of the ring of radius `radius` at `angle` (radians): the same frame the angles
    /// are measured in.
    QVector3D ringPoint(double angle, double radius) const {
        return centre_ + radius * (std::cos(angle) * u_ + std::sin(angle) * v_);
    }
    /// The frame of the ring round an axis: `u`, `v` with u × v = axis.
    static void frameOf(const QVector3D& axis, QVector3D& u, QVector3D& v) { basisOf(axis, u, v); }

private:
    static void basisOf(const QVector3D& axis, QVector3D& u, QVector3D& v);

    QVector3D centre_;
    QVector3D axis_;
    QVector3D u_; // the plane of the ring: u × v = axis
    QVector3D v_;
    double first_ = 0.0; // the angle at the grab
    double last_ = 0.0;  // the angle the pointer had last (radians, in -pi … pi)
    double total_ = 0.0; // what has been turned since the grab (radians)
    bool onScreen_ = false;
    QPointF start_;   // where the pointer went down (screen mode)
    QPointF tangent_; // the way the ring runs on the screen there (unit vector)
    double pixelsPerRadian_ = 1.0;
};

/// The turning step: 15 degrees.
inline constexpr double kTurnStepDegrees = 15.0;

/// The step a dragged length snaps to: whole millimetres, finer when zoomed in, coarser only when
/// a millimetre is hardly a pixel on screen.
double snapStep(double pixelsPerMillimetre);

/// `value` on the nearest multiple of `step` (`step` > 0).
double snapped(double value, double step);

template <typename Project>
std::optional<RingDrag> RingDrag::grab(const Ray& ray, const QPointF& pointer,
                                       const QVector3D& centre, const QVector3D& axis,
                                       Project project, double radius) {
    if (axis.lengthSquared() < 1e-12F) {
        return std::nullopt;
    }
    RingDrag drag;
    drag.centre_ = centre;
    drag.axis_ = axis.normalized();
    basisOf(drag.axis_, drag.u_, drag.v_);
    drag.start_ = pointer;
    const float facing = QVector3D::dotProduct(ray.direction, drag.axis_);
    if (std::abs(facing) >= kLeastRingSight) {
        // Seen from a good angle: where the ray meets the plane of the ring.
        const float reach = QVector3D::dotProduct(centre - ray.origin, drag.axis_) / facing;
        if (reach <= 0.0F) {
            return std::nullopt;
        }
        const QVector3D met = ray.origin + ray.direction * reach - centre;
        drag.last_ =
            std::atan2(QVector3D::dotProduct(met, drag.v_), QVector3D::dotProduct(met, drag.u_));
        drag.first_ = drag.last_;
        return drag;
    }
    // Edge on, the ring is (almost) a line on the screen and is turned by dragging along that
    // line, like a wheel seen from the side. The line runs along `across`: the way the ring's
    // point nearest to the eye moves when it turns. It lies in the ring's plane and square to
    // the line of sight, so it is always seen in full. (The tangent where the pointer is would
    // not do: where the line ends on the screen, the ring runs straight into the screen.)
    drag.onScreen_ = true;
    const QVector3D sight = ray.direction.normalized();
    const QVector3D towardEye = QVector3D::dotProduct(sight, drag.axis_) * drag.axis_ - sight;
    if (towardEye.lengthSquared() < 1e-12F) {
        return std::nullopt; // cannot be while edge on, but never divide by nothing
    }
    const QVector3D nearest = towardEye.normalized();
    const QVector3D across = QVector3D::crossProduct(drag.axis_, nearest);
    const float along = QVector3D::dotProduct(centre - ray.origin, sight);
    if (along <= 0.0F) {
        return std::nullopt; // the middle is behind the eye
    }
    // How far along the line the pointer is: the same for every point of the ray.
    const float side = QVector3D::dotProduct(ray.origin + sight * along - centre, across);
    float ring = static_cast<float>(radius);
    if (ring <= 0.0F) {
        // No size given: how far from the middle the ray meets the plane of the ring (a shallow
        // angle is good enough for that); a ray running in the plane, how far along the line.
        ring = std::abs(side);
        const float facingNow = QVector3D::dotProduct(sight, drag.axis_);
        if (std::abs(facingNow) > 1e-4F) {
            const float meet = QVector3D::dotProduct(centre - ray.origin, drag.axis_) / facingNow;
            if (meet > 0.0F) {
                QVector3D met = ray.origin + sight * meet - centre;
                met -= QVector3D::dotProduct(met, drag.axis_) * drag.axis_;
                ring = met.length();
            }
        }
    }
    // The grabbed point lies under the pointer on the near half of the ring.
    const float reach = std::clamp(side, -ring, ring);
    const QVector3D grabbed =
        reach * across + std::sqrt(std::max(ring * ring - reach * reach, 0.0F)) * nearest;
    if (grabbed.lengthSquared() > 1e-12F) {
        drag.first_ = std::atan2(QVector3D::dotProduct(grabbed, drag.v_),
                                 QVector3D::dotProduct(grabbed, drag.u_));
        drag.last_ = drag.first_;
    }
    const QPointF here = project(centre);
    const QPointF there = project(centre + across);
    const QPointF way = there - here;
    const double perMillimetre = std::hypot(way.x(), way.y());
    drag.tangent_ = perMillimetre > 1e-9 ? way / perMillimetre : QPointF(1.0, 0.0);
    // One ring radius along the line is one radian, wherever on the line the ring was taken.
    drag.pixelsPerRadian_ = std::max(static_cast<double>(ring) * perMillimetre, kLeastScreenRadius);
    return drag;
}

} // namespace fission::interaction
