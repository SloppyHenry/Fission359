// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/drag.h"

#include <cmath>

namespace fission::interaction {

namespace {

/// Where the ray meets the plane through `point` with normal `normal`, if in front of the eye.
std::optional<QVector3D> meet(const Ray& ray, const QVector3D& point, const QVector3D& normal) {
    const float denominator = QVector3D::dotProduct(ray.direction, normal);
    if (std::abs(denominator) < 1e-6F) {
        return std::nullopt;
    }
    const float reach = QVector3D::dotProduct(point - ray.origin, normal) / denominator;
    if (reach <= 0.0F) {
        return std::nullopt; // behind the eye
    }
    return ray.origin + ray.direction * reach;
}

} // namespace

std::optional<AxisDrag> AxisDrag::grab(const Ray& ray, const QVector3D& origin,
                                       const QVector3D& axis, const Screen& screen,
                                       double leastSight) {
    if (axis.lengthSquared() < 1e-12F) {
        return std::nullopt;
    }
    AxisDrag drag;
    drag.origin_ = origin;
    drag.axis_ = axis.normalized();
    drag.screen_ = screen;
    // The plane through the axis that faces the viewer most: the part of the line of sight that
    // does not run along the axis.
    const QVector3D across =
        ray.direction - QVector3D::dotProduct(ray.direction, drag.axis_) * drag.axis_;
    if (across.length() < leastSight) {
        drag.onScreen_ = true; // points at the viewer: dragged on the screen
        return drag;
    }
    drag.plane_ = across.normalized();
    const auto met = meet(ray, origin, drag.plane_);
    if (!met) {
        return std::nullopt;
    }
    drag.grabbed_ = QVector3D::dotProduct(*met - origin, drag.axis_);
    drag.reach_ = (*met - ray.origin).length();
    return drag;
}

std::optional<double> AxisDrag::moved(const Ray& ray, const QPointF& pointer) const {
    if (onScreen_) {
        const QPointF way = pointer - screen_.pointer;
        return (way.x() * screen_.way.x() + way.y() * screen_.way.y()) /
               screen_.pixelsPerMillimetre;
    }
    const auto met = meet(ray, origin_, plane_);
    if (!met || (*met - ray.origin).length() > kFarthestReach * reach_) {
        return std::nullopt;
    }
    return QVector3D::dotProduct(*met - origin_, axis_) - grabbed_;
}

std::optional<PlaneDrag> PlaneDrag::grab(const Ray& ray, const QVector3D& origin,
                                         const QVector3D& normal, double leastSight) {
    if (normal.lengthSquared() < 1e-12F) {
        return std::nullopt;
    }
    PlaneDrag drag;
    drag.normal_ = normal.normalized();
    if (std::abs(QVector3D::dotProduct(ray.direction, drag.normal_)) < leastSight) {
        return std::nullopt;
    }
    const auto met = meet(ray, origin, drag.normal_);
    if (!met) {
        return std::nullopt;
    }
    drag.grabbed_ = *met;
    drag.reach_ = (*met - ray.origin).length();
    return drag;
}

std::optional<QVector3D> PlaneDrag::moved(const Ray& ray) const {
    const auto met = meet(ray, grabbed_, normal_);
    if (!met || (*met - ray.origin).length() > kFarthestReach * reach_) {
        return std::nullopt;
    }
    QVector3D offset = *met - grabbed_;
    // Exactly in the plane (no drift out of it from rounding).
    return offset - QVector3D::dotProduct(offset, normal_) * normal_;
}

void RingDrag::basisOf(const QVector3D& axis, QVector3D& u, QVector3D& v) {
    // X: (Y, Z), Y: (Z, X), Z: (X, Y): u × v is the axis for a right-handed frame.
    const float ax = std::abs(axis.x());
    const float ay = std::abs(axis.y());
    const float az = std::abs(axis.z());
    QVector3D start = (ax <= ay && ax <= az)   ? QVector3D(1, 0, 0)
                      : (ay <= ax && ay <= az) ? QVector3D(0, 1, 0)
                                               : QVector3D(0, 0, 1);
    if (axis.x() > 0.999F) {
        start = QVector3D(0, 1, 0);
    } else if (axis.y() > 0.999F) {
        start = QVector3D(0, 0, 1);
    } else if (axis.z() > 0.999F) {
        start = QVector3D(1, 0, 0);
    }
    u = (start - QVector3D::dotProduct(start, axis) * axis).normalized();
    v = QVector3D::crossProduct(axis, u);
}

std::optional<double> RingDrag::moved(const Ray& ray, const QPointF& pointer) {
    constexpr double kPi = 3.14159265358979323846;
    if (onScreen_) {
        const QPointF way = pointer - start_;
        total_ = (way.x() * tangent_.x() + way.y() * tangent_.y()) / pixelsPerRadian_;
        return total_ * 180.0 / kPi;
    }
    const float facing = QVector3D::dotProduct(ray.direction, axis_);
    if (std::abs(facing) < 1e-6F) {
        return std::nullopt;
    }
    const float reach = QVector3D::dotProduct(centre_ - ray.origin, axis_) / facing;
    if (reach <= 0.0F) {
        return std::nullopt; // behind the eye
    }
    const QVector3D met = ray.origin + ray.direction * reach - centre_;
    const double angle = std::atan2(QVector3D::dotProduct(met, v_), QVector3D::dotProduct(met, u_));
    double step = angle - last_;
    step -= 2.0 * kPi * std::round(step / (2.0 * kPi)); // the short way round
    total_ += step;
    last_ = angle;
    return total_ * 180.0 / kPi;
}

double snapStep(double pixelsPerMillimetre) {
    // Whole millimetres are the everyday step: a corner radius of 0 to 10 mm must not come in
    // jumps of 5. Finer when zoomed in, coarser only when the handle is hardly longer than a
    // pixel per millimetre.
    if (pixelsPerMillimetre >= 20.0) {
        return 0.1;
    }
    if (pixelsPerMillimetre >= 6.0) {
        return 0.5;
    }
    if (pixelsPerMillimetre >= 0.8) {
        return 1.0;
    }
    return pixelsPerMillimetre >= 0.3 ? 5.0 : 10.0;
}

double snapped(double value, double step) {
    const double result = std::round(value / step) * step;
    return result == 0.0 ? 0.0 : result; // no "-0"
}

} // namespace fission::interaction
