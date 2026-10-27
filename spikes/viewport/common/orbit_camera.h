// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4: orbit camera shared by the prototypes (Z up, like a 3D printer bed).

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fission::spike {

class OrbitCamera {
public:
    void frame(const QVector3D& boundsMin, const QVector3D& boundsMax) {
        m_target = 0.5F * (boundsMin + boundsMax);
        m_radius = 0.5F * (boundsMax - boundsMin).length();
        m_distance = m_radius / std::tan(0.5F * kFov) * 1.15F;
    }

    void orbit(float dxPixels, float dyPixels) {
        m_yaw -= dxPixels * 0.35F;
        m_pitch = std::clamp(m_pitch + dyPixels * 0.35F, -89.0F, 89.0F);
    }

    void pan(float dxPixels, float dyPixels, float viewportHeight) {
        const float perPixel = 2.0F * m_distance * std::tan(0.5F * kFov) / viewportHeight;
        const QMatrix4x4 inverse = view().inverted();
        const QVector3D right = inverse.mapVector(QVector3D(1, 0, 0));
        const QVector3D up = inverse.mapVector(QVector3D(0, 1, 0));
        m_target += (-dxPixels * right + dyPixels * up) * perPixel;
    }

    void zoom(float factor) { m_distance = std::clamp(m_distance * factor, m_radius * 0.02F, m_radius * 60.0F); }

    QVector3D eye() const {
        const float yaw = m_yaw * std::numbers::pi_v<float> / 180.0F;
        const float pitch = m_pitch * std::numbers::pi_v<float> / 180.0F;
        const QVector3D offset(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw),
                               std::sin(pitch));
        return m_target + offset * m_distance;
    }

    QMatrix4x4 view() const {
        QMatrix4x4 matrix;
        matrix.lookAt(eye(), m_target, QVector3D(0, 0, 1));
        return matrix;
    }

    QMatrix4x4 projection(float aspect) const {
        QMatrix4x4 matrix;
        const float near = std::max(m_distance - m_radius * 1.5F, m_radius * 0.01F);
        const float far = m_distance + m_radius * 2.0F;
        matrix.perspective(kFov * 180.0F / std::numbers::pi_v<float>, aspect, near, far);
        return matrix;
    }

    const QVector3D& target() const { return m_target; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    void setAngles(float yaw, float pitch) { m_yaw = yaw; m_pitch = pitch; }

private:
    static constexpr float kFov = 45.0F * std::numbers::pi_v<float> / 180.0F;
    QVector3D m_target{0, 0, 0};
    float m_radius = 100.0F;
    float m_distance = 300.0F;
    float m_yaw = -50.0F;
    float m_pitch = 30.0F;
};

} // namespace fission::spike
