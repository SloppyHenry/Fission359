// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QSizeF>
#include <QVector3D>

#include <optional>

namespace fission::render {

/// The standard views of the view cube and the keys 1-7 (docs/02-ux-konzept.md §10).
enum class StandardView { home, top, bottom, front, back, left, right };

/// Where the camera stands: the point it looks at, the distance and two angles in degrees.
struct CameraPose {
    QVector3D target{0.0F, 0.0F, 0.0F};
    float distance = 400.0F;
    float yaw = -30.0F;  ///< Turn around the vertical axis; 0 = looking from the front (-Y).
    float pitch = 36.0F; ///< Elevation above the bed; 90 = looking straight down.

    bool operator==(const CameraPose&) const = default;
};

/// Orbit camera for a Z-up world (like a 3D printer bed). Pure mathematics, no graphics API.
///
/// Mouse: orbit, pan and zoom as described in docs/02-ux-konzept.md §10. Zooming goes towards
/// the point under the pointer (on the bed plane), so what you look at stays where it is.
class OrbitCamera {
public:
    /// Size of the view in logical pixels; needed for panning and zooming to the pointer.
    void setViewportSize(const QSizeF& size);
    QSizeF viewportSize() const { return m_viewport; }

    /// Frames a box so that it fills the view with a little margin; keeps the angles.
    void frame(const QVector3D& boundsMin, const QVector3D& boundsMax);

    void orbit(float dxPixels, float dyPixels);
    void pan(float dxPixels, float dyPixels);
    /// factor < 1 moves closer. `cursor` is in view pixels; without it the view centre is used.
    void zoomAt(float factor, std::optional<QPointF> cursor = std::nullopt);

    void setOrthographic(bool orthographic) { m_orthographic = orthographic; }
    bool orthographic() const { return m_orthographic; }

    CameraPose pose() const { return m_pose; }
    void setPose(const CameraPose& pose);
    /// The pose of a standard view, keeping the current target and distance.
    CameraPose poseFor(StandardView view) const;

    QVector3D eye() const;
    QMatrix4x4 view() const;
    QMatrix4x4 projection(float aspect) const;

    /// Pixels per millimetre at the target (decides how fine the bed grid is).
    float pixelsPerMillimetre() const;

    /// The point where the ray through `pixel` hits the plane z = planeZ, if it does.
    std::optional<QVector3D> pointOnPlane(const QPointF& pixel, float planeZ = 0.0F) const;
    /// Window position (view pixels) of a world point.
    QPointF project(const QVector3D& world) const;

    static constexpr float kFieldOfViewDegrees = 45.0F;

private:
    float aspect() const;

    CameraPose m_pose;
    QSizeF m_viewport{800.0, 600.0};
    float m_sceneRadius = 200.0F;
    bool m_orthographic = false;
};

/// Linear blend of two poses (angles take the short way round), for animated view changes.
CameraPose blend(const CameraPose& from, const CameraPose& to, float t);

} // namespace fission::render
