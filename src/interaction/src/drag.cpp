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
                                       const QVector3D& axis, double leastSight) {
    if (axis.lengthSquared() < 1e-12F) {
        return std::nullopt;
    }
    AxisDrag drag;
    drag.origin_ = origin;
    drag.axis_ = axis.normalized();
    // The plane through the axis that faces the viewer most: the part of the line of sight that
    // does not run along the axis.
    const QVector3D across =
        ray.direction - QVector3D::dotProduct(ray.direction, drag.axis_) * drag.axis_;
    if (across.length() < leastSight) {
        return std::nullopt;
    }
    drag.plane_ = across.normalized();
    const auto met = meet(ray, origin, drag.plane_);
    if (!met) {
        return std::nullopt;
    }
    drag.grabbed_ = QVector3D::dotProduct(*met - origin, drag.axis_);
    return drag;
}

std::optional<double> AxisDrag::moved(const Ray& ray) const {
    const auto met = meet(ray, origin_, plane_);
    if (!met) {
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
    return drag;
}

std::optional<QVector3D> PlaneDrag::moved(const Ray& ray) const {
    const auto met = meet(ray, grabbed_, normal_);
    if (!met) {
        return std::nullopt;
    }
    QVector3D offset = *met - grabbed_;
    // Exactly in the plane (no drift out of it from rounding).
    return offset - QVector3D::dotProduct(offset, normal_) * normal_;
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
