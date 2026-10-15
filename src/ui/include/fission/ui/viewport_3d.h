// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The 3D view as a Qt Quick item: draws the print bed and the bodies with the RHI renderer and
// takes mouse, trackpad and wheel input (docs/02-ux-konzept.md §10).

#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickRhiItem>
#include <QString>
#include <QVariantAnimation>

#include <memory>
#include <optional>
#include <vector>

#include "fission/render/camera.h"
#include "fission/render/navigation.h"
#include "fission/render/picking.h"
#include "fission/render/scene.h"
#include "fission/render/scene_renderer.h"

namespace fission::ui {

class Viewport3D : public QQuickRhiItem {
    Q_OBJECT
    Q_PROPERTY(QColor viewCenter MEMBER m_viewCenter NOTIFY paletteChanged)
    Q_PROPERTY(QColor viewEdge MEMBER m_viewEdge NOTIFY paletteChanged)
    Q_PROPERTY(QColor bedColor MEMBER m_bed NOTIFY paletteChanged)
    Q_PROPERTY(QColor bedEdgeColor MEMBER m_bedEdge NOTIFY paletteChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_grid NOTIFY paletteChanged)
    Q_PROPERTY(QColor gridMajorColor MEMBER m_gridMajor NOTIFY paletteChanged)
    Q_PROPERTY(QColor axisXColor MEMBER m_axisX NOTIFY paletteChanged)
    Q_PROPERTY(QColor axisYColor MEMBER m_axisY NOTIFY paletteChanged)
    Q_PROPERTY(QColor originColor MEMBER m_origin NOTIFY paletteChanged)
    Q_PROPERTY(QColor edgeColor MEMBER m_edgeLine NOTIFY paletteChanged)
    Q_PROPERTY(QColor shadowColor MEMBER m_shadow NOTIFY paletteChanged)
    Q_PROPERTY(bool bedVisible READ bedVisible WRITE setBedVisible NOTIFY bedVisibleChanged)
    Q_PROPERTY(bool orthographic READ orthographic WRITE setOrthographic NOTIFY orthographicChanged)
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(bool animated MEMBER m_animated)
    /// Soft shadows in corners ("Weiche Schatten"). Turns itself off when the graphics card
    /// cannot keep 60 frames per second with it (then `ambientOcclusionTurnedOff` is emitted).
    Q_PROPERTY(bool ambientOcclusion READ ambientOcclusion WRITE setAmbientOcclusion NOTIFY
                   ambientOcclusionChanged)
    /// "fission", "fusion", "tinkercad", "blender" or "sketchup": which gestures move the view.
    Q_PROPERTY(QString mouseStyle READ mouseStyle WRITE setMouseStyle NOTIFY mouseStyleChanged)
    Q_PROPERTY(double cameraYaw READ cameraYaw NOTIFY cameraChanged)
    Q_PROPERTY(double cameraPitch READ cameraPitch NOTIFY cameraChanged)
    Q_PROPERTY(double pixelsPerMillimetre READ pixelsPerMillimetre NOTIFY cameraChanged)
    Q_PROPERTY(QString backendName READ backendName NOTIFY backendNameChanged)
    Q_PROPERTY(int triangleCount READ triangleCount NOTIFY sceneChanged)
    Q_PROPERTY(double bedSizeX READ bedSizeX NOTIFY sceneChanged)
    Q_PROPERTY(double bedSizeY READ bedSizeY NOTIFY sceneChanged)

public:
    explicit Viewport3D(QQuickItem* parent = nullptr);

    /// The scene to show; the item keeps it alive. Safe to call from the GUI thread at any time.
    void setScene(std::shared_ptr<const render::RenderScene> scene);

    bool bedVisible() const { return m_bedVisible; }
    void setBedVisible(bool visible);
    bool orthographic() const { return m_camera.orthographic(); }
    void setOrthographic(bool orthographic);
    bool ambientOcclusion() const { return m_ambientOcclusion; }
    void setAmbientOcclusion(bool on);
    /// Called by the renderer (render thread) when frames take too long with occlusion on.
    void reportSlowFrames(double averageMilliseconds);
    QString mouseStyle() const;
    void setMouseStyle(const QString& name);
    QString displayMode() const { return m_displayMode; }
    void setDisplayMode(const QString& mode);
    double cameraYaw() const { return m_camera.pose().yaw; }
    double cameraPitch() const { return m_camera.pose().pitch; }
    double pixelsPerMillimetre() const { return m_camera.pixelsPerMillimetre(); }
    QString backendName() const { return m_backendName; }
    int triangleCount() const;
    double bedSizeX() const;
    double bedSizeY() const;

    /// "home", "top", "bottom", "front", "back", "left" or "right".
    Q_INVOKABLE void setStandardView(const QString& name);
    /// Looks at the target from the side the vector points to (x, y, z in the world, any
    /// length): the edges and corners of the view cube.
    Q_INVOKABLE void setViewDirection(double x, double y, double z);
    /// Turns the view to look squarely at the surface under the point (x, y) in item pixels and
    /// makes the face the new centre. Returns false if there is nothing under the pointer.
    Q_INVOKABLE bool lookAtSurface(double x, double y);
    /// Moves the face under the point to the middle and zooms to fit it, without turning the
    /// view (Smart Zoom of the trackpad).
    Q_INVOKABLE bool zoomToSurface(double x, double y);
    /// Puts the camera at once (no animation) at the given angles and distance from its target.
    Q_INVOKABLE void setCamera(double yaw, double pitch, double distance);
    /// Shows or hides one body of the scene (the eye in the body list).
    Q_INVOKABLE void setBodyVisible(int index, bool visible);
    /// Frames the scene again (key F).
    Q_INVOKABLE void frameAll();
    /// Zoom by `factor` (< 1 moves closer) towards the point (x, y) in item pixels.
    Q_INVOKABLE void zoomBy(double factor, double x = -1.0, double y = -1.0);
    /// Pan by pixels, like the pan button.
    Q_INVOKABLE void panBy(double dx, double dy);
    /// Orbit by pixels, like the rotate button.
    Q_INVOKABLE void orbitBy(double dx, double dy);
    /// Position of a world point in item pixels (for labels that float in the 3D view).
    Q_INVOKABLE QPointF project(double x, double y, double z) const;

    // --- used by the renderer (render thread while the GUI thread waits in synchronize) ---
    struct Snapshot {
        std::shared_ptr<const render::RenderScene> scene;
        render::ViewPalette palette;
        QMatrix4x4 view;
        QMatrix4x4 projection;
        float pixelsPerMillimetre = 2.0F;
        bool bedVisible = true;
        render::DisplayMode displayMode = render::DisplayMode::colors;
        bool ambientOcclusion = true;
        quint64 sceneVersion = 0;
        quint64 paletteVersion = 0;
    };
    Snapshot takeSnapshot(float aspect);
    void reportRendered(const QString& backendName);

signals:
    void paletteChanged();
    void bedVisibleChanged();
    void orthographicChanged();
    void mouseStyleChanged();
    void ambientOcclusionChanged();
    /// The soft shadows were switched off because the frames took too long.
    void ambientOcclusionTurnedOff(double averageMilliseconds);
    /// The view needs finer (or may use coarser) tessellation at the current zoom.
    void detailLevelWanted(int level);
    void displayModeChanged();
    void cameraChanged();
    void sceneChanged();
    void backendNameChanged();
    /// The first frame with the 3D view has been handed to the display.
    void firstFrameShown();

protected:
    QQuickRhiItemRenderer* createRenderer() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    render::ViewPalette palette() const;
    void animateTo(const render::CameraPose& target);
    void cameraMoved();

    void rebuildVisibleScene();

    std::shared_ptr<const render::RenderScene> m_scene; // everything
    std::shared_ptr<const render::RenderScene> m_shown; // without hidden bodies
    std::vector<bool> m_bodyVisible;
    void applyNavigation(render::NavigationAction action, double dx, double dy);
    std::optional<render::SurfaceHit> surfaceAt(double x, double y) const;

    bool m_ambientOcclusion = true;
    int m_detailLevel = 0;
    render::MouseStyle m_mouseStyle = render::MouseStyle::fission;
    QString m_displayMode{QStringLiteral("color")};
    quint64 m_sceneVersion = 1;
    quint64 m_paletteVersion = 1;
    render::OrbitCamera m_camera;
    QPointF m_lastMouse;
    bool m_bedVisible = true;
    bool m_animated = true;
    bool m_firstFrameReported = false;
    QString m_backendName;
    QVariantAnimation m_animation;
    render::CameraPose m_animationFrom;
    render::CameraPose m_animationTo;

    QColor m_viewCenter{0x2B, 0x2D, 0x31};
    QColor m_viewEdge{0x23, 0x25, 0x28};
    QColor m_bed{0x25, 0x27, 0x2B};
    QColor m_bedEdge{0x44, 0x47, 0x4E};
    QColor m_grid{0x30, 0x32, 0x37};
    QColor m_gridMajor{0x3A, 0x3D, 0x43};
    QColor m_axisX{0xE5, 0x53, 0x4B};
    QColor m_axisY{0x57, 0xAB, 0x5A};
    QColor m_origin{0xA4, 0xA8, 0xB0};
    QColor m_edgeLine{10, 11, 13, 140};
    QColor m_shadow{0, 0, 0, 153};
};

/// Makes the QML type "Viewport3D" (import Fission3D.View) available.
void registerViewportTypes();

} // namespace fission::ui
