// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fission::render {

namespace {

constexpr float kDegrees = std::numbers::pi_v<float> / 180.0F;
constexpr float kOrbitDegreesPerPixel = 0.35F;
constexpr float kMaxPitch = 89.5F;

float tanHalfFov() {
    return std::tan(0.5F * OrbitCamera::kFieldOfViewDegrees * kDegrees);
}

float wrapDegrees(float angle) {
    angle = std::fmod(angle + 180.0F, 360.0F);
    if (angle < 0.0F) {
        angle += 360.0F;
    }
    return angle - 180.0F;
}

} // namespace

void OrbitCamera::setViewportSize(const QSizeF& size) {
    if (size.width() > 0.0 && size.height() > 0.0) {
        m_viewport = size;
    }
}

float OrbitCamera::aspect() const {
    return static_cast<float>(m_viewport.width() / m_viewport.height());
}

void OrbitCamera::frame(const QVector3D& boundsMin, const QVector3D& boundsMax) {
    m_pose.target = 0.5F * (boundsMin + boundsMax);
    m_sceneRadius = std::max(0.5F * (boundsMax - boundsMin).length(), 1.0F);
    // The sphere around the box must fit, also in a narrow window (then the width decides).
    const float tanLimit = tanHalfFov() * std::min(1.0F, aspect());
    m_pose.distance = m_sceneRadius / std::sin(std::atan(tanLimit)) * 1.1F;
}

void OrbitCamera::orbit(float dxPixels, float dyPixels) {
    // Like turning a table: the scene follows the pointer.
    m_pose.yaw = wrapDegrees(m_pose.yaw + dxPixels * kOrbitDegreesPerPixel);
    m_pose.pitch =
        std::clamp(m_pose.pitch + dyPixels * kOrbitDegreesPerPixel, -kMaxPitch, kMaxPitch);
}

void OrbitCamera::pan(float dxPixels, float dyPixels) {
    const float perPixel =
        2.0F * m_pose.distance * tanHalfFov() / static_cast<float>(m_viewport.height());
    const QMatrix4x4 inverse = view().inverted();
    const QVector3D right = inverse.mapVector(QVector3D(1, 0, 0));
    const QVector3D up = inverse.mapVector(QVector3D(0, 1, 0));
    m_pose.target += (-dxPixels * right + dyPixels * up) * perPixel;
}

void OrbitCamera::zoomAt(float factor, std::optional<QPointF> cursor) {
    const float minimum = 2.0F;
    const float maximum = std::max(m_sceneRadius * 40.0F, 2000.0F);
    const std::optional<QVector3D> before = cursor ? pointOnPlane(*cursor) : std::nullopt;

    m_pose.distance = std::clamp(m_pose.distance * factor, minimum, maximum);

    // Keep the point under the pointer under the pointer: shift the target by what moved.
    if (before) {
        if (const std::optional<QVector3D> after = pointOnPlane(*cursor)) {
            m_pose.target += *before - *after;
        }
    }
}

void OrbitCamera::setPose(const CameraPose& pose) {
    m_pose = pose;
    m_pose.pitch = std::clamp(m_pose.pitch, -kMaxPitch, kMaxPitch);
}

CameraPose OrbitCamera::poseFor(StandardView standard) const {
    CameraPose pose = m_pose;
    switch (standard) {
    case StandardView::home:
        pose.yaw = -30.0F;
        pose.pitch = 36.0F;
        break;
    case StandardView::top:
        pose.yaw = 0.0F;
        pose.pitch = kMaxPitch;
        break;
    case StandardView::bottom:
        pose.yaw = 0.0F;
        pose.pitch = -kMaxPitch;
        break;
    case StandardView::front:
        pose.yaw = 0.0F;
        pose.pitch = 0.0F;
        break;
    case StandardView::back:
        pose.yaw = 180.0F;
        pose.pitch = 0.0F;
        break;
    case StandardView::left:
        pose.yaw = 90.0F;
        pose.pitch = 0.0F;
        break;
    case StandardView::right:
        pose.yaw = -90.0F;
        pose.pitch = 0.0F;
        break;
    }
    return pose;
}

QVector3D OrbitCamera::eye() const {
    // yaw 0 looks from the front (the -Y side); positive yaw turns the camera to the left.
    const float yaw = m_pose.yaw * kDegrees;
    const float pitch = m_pose.pitch * kDegrees;
    const QVector3D offset(-std::cos(pitch) * std::sin(yaw), -std::cos(pitch) * std::cos(yaw),
                           std::sin(pitch));
    return m_pose.target + offset * m_pose.distance;
}

QMatrix4x4 OrbitCamera::view() const {
    QMatrix4x4 matrix;
    // Looking straight down, "up" would be ambiguous: use the horizontal heading instead.
    QVector3D up(0.0F, 0.0F, 1.0F);
    if (std::abs(m_pose.pitch) > 89.0F) {
        const float yaw = m_pose.yaw * kDegrees;
        const float sign = m_pose.pitch > 0.0F ? 1.0F : -1.0F;
        up = QVector3D(sign * std::sin(yaw), sign * std::cos(yaw), 0.0F);
    }
    matrix.lookAt(eye(), m_pose.target, up);
    return matrix;
}

QMatrix4x4 OrbitCamera::projection(float aspectRatio) const {
    QMatrix4x4 matrix;
    const float nearPlane =
        std::max(m_pose.distance - m_sceneRadius * 1.5F, m_pose.distance * 0.02F);
    const float farPlane = m_pose.distance + m_sceneRadius * 2.5F;
    if (m_orthographic) {
        const float halfHeight = m_pose.distance * tanHalfFov();
        const float halfWidth = halfHeight * aspectRatio;
        matrix.ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
    } else {
        matrix.perspective(kFieldOfViewDegrees, aspectRatio, nearPlane, farPlane);
    }
    return matrix;
}

float OrbitCamera::pixelsPerMillimetre() const {
    return static_cast<float>(m_viewport.height()) / (2.0F * m_pose.distance * tanHalfFov());
}

std::optional<QVector3D> OrbitCamera::pointOnPlane(const QPointF& pixel, float planeZ) const {
    const float nx = static_cast<float>(2.0 * pixel.x() / m_viewport.width() - 1.0);
    const float ny = static_cast<float>(1.0 - 2.0 * pixel.y() / m_viewport.height());
    bool invertible = false;
    const QMatrix4x4 inverse = (projection(aspect()) * view()).inverted(&invertible);
    if (!invertible) {
        return std::nullopt;
    }
    const QVector3D nearPoint = inverse.map(QVector3D(nx, ny, -1.0F));
    const QVector3D farPoint = inverse.map(QVector3D(nx, ny, 1.0F));
    const QVector3D direction = farPoint - nearPoint;
    if (std::abs(direction.z()) < 1e-6F) {
        return std::nullopt;
    }
    const float t = (planeZ - nearPoint.z()) / direction.z();
    // Behind the camera or absurdly far (near the horizon): not a usable target.
    if (t < 0.0F || t > 1.0F) {
        return std::nullopt;
    }
    return nearPoint + direction * t;
}

QPointF OrbitCamera::project(const QVector3D& world) const {
    const QVector3D ndc = (projection(aspect()) * view()).map(world);
    return {(static_cast<double>(ndc.x()) * 0.5 + 0.5) * m_viewport.width(),
            (0.5 - static_cast<double>(ndc.y()) * 0.5) * m_viewport.height()};
}

CameraPose blend(const CameraPose& from, const CameraPose& to, float t) {
    CameraPose pose;
    pose.target = from.target + (to.target - from.target) * t;
    pose.distance = from.distance + (to.distance - from.distance) * t;
    const float yawDelta = wrapDegrees(to.yaw - from.yaw);
    pose.yaw = wrapDegrees(from.yaw + yawDelta * t);
    pose.pitch = from.pitch + (to.pitch - from.pitch) * t;
    return pose;
}

} // namespace fission::render
