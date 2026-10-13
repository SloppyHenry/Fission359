// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype B: OpenCASCADE's AIS/V3d viewer embedded in Qt Quick through a
// framebuffer object (QQuickFramebufferObject, OpenGL only).

#pragma once

#include "../common/orbit_camera.h"
#include "../common/scene.h"

#include <QElapsedTimer>
#include <QPoint>
#include <QQuickFramebufferObject>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

namespace fission::spike {

class AisViewport : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int triangles READ triangles NOTIFY sceneChanged)
    Q_PROPERTY(int faceCount READ faceCount NOTIFY sceneChanged)
    Q_PROPERTY(int hoverFace READ hoverFace NOTIFY hoverFaceChanged)
    Q_PROPERTY(int selectedFace READ selectedFace NOTIFY selectedFaceChanged)
    Q_PROPERTY(double fps READ fps NOTIFY statsChanged)
    Q_PROPERTY(double pickLatencyMs READ pickLatencyMs NOTIFY statsChanged)
    Q_PROPERTY(QString backend READ backend CONSTANT)

public:
    explicit AisViewport(QQuickItem* parent = nullptr);

    int triangles() const { return m_scene ? static_cast<int>(m_scene->mesh.triangleCount()) : 0; }
    int faceCount() const { return m_scene ? m_scene->faceCount : 0; }
    int hoverFace() const { return m_hoverFace; }
    int selectedFace() const { return m_selectedFace; }
    double fps() const { return m_fps; }
    double pickLatencyMs() const { return m_pickLatencyMs; }
    QString backend() const { return QStringLiteral("OpenGL (OpenCASCADE AIS)"); }

    Q_INVOKABLE void zoomBy(double factor);

    Renderer* createRenderer() const override;

    // --- used by the renderer (render thread while the GUI thread is blocked) ---
    struct Snapshot {
        std::shared_ptr<const Scene> scene;
        QVector3D eye;
        QVector3D target;
        bool pickRequested = false;
        QPoint pickPixel;
        bool clicked = false;
        qint64 requestedAtNs = 0;
    };
    Snapshot takeSnapshot();
    void reportFrame();
    void reportPick(int faceId, bool selected, double latencyMs);
    qint64 nowNs() const { return m_clock.nsecsElapsed(); }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;

signals:
    void sceneChanged();
    void hoverFaceChanged();
    void selectedFaceChanged();
    void statsChanged();

private:
    std::shared_ptr<const Scene> m_scene;
    OrbitCamera m_camera;
    QPointF m_lastMouse;
    bool m_dragged = false;
    bool m_pickWanted = false;
    bool m_clickWanted = false;
    QPoint m_pickPixel;
    qint64 m_pickWantedAtNs = 0;
    QElapsedTimer m_clock;

    int m_hoverFace = 0;
    int m_selectedFace = 0;
    double m_fps = 0.0;
    double m_pickLatencyMs = 0.0;
    qint64 m_lastFrameNs = 0;
    qint64 m_lastStatsNs = 0;
    std::vector<double> m_intervalsMs;
    int m_screenshotFrames = 0;
};

} // namespace fission::spike
