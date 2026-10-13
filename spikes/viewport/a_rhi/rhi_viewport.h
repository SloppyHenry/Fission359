// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype A: a Qt Quick item that draws the scene with Qt RHI (Metal, Vulkan,
// OpenGL, D3D) and picks faces through an ID buffer.

#pragma once

#include "../common/orbit_camera.h"
#include "../common/scene.h"

#include <QElapsedTimer>
#include <QPoint>
#include <QQuickRhiItem>
#include <QtQml/qqmlregistration.h>

#include <chrono>
#include <memory>
#include <vector>

namespace fission::spike {

class RhiViewport : public QQuickRhiItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int triangles READ triangles NOTIFY sceneChanged)
    Q_PROPERTY(int faceCount READ faceCount NOTIFY sceneChanged)
    Q_PROPERTY(int hoverFace READ hoverFace NOTIFY hoverFaceChanged)
    Q_PROPERTY(int selectedFace READ selectedFace NOTIFY selectedFaceChanged)
    Q_PROPERTY(double fps READ fps NOTIFY statsChanged)
    Q_PROPERTY(double gpuMs READ gpuMs NOTIFY statsChanged)
    Q_PROPERTY(double pickLatencyMs READ pickLatencyMs NOTIFY statsChanged)
    Q_PROPERTY(QString backend READ backend NOTIFY statsChanged)

public:
    explicit RhiViewport(QQuickItem* parent = nullptr);

    int triangles() const { return m_scene ? static_cast<int>(m_scene->mesh.triangleCount()) : 0; }
    int faceCount() const { return m_scene ? m_scene->faceCount : 0; }
    int hoverFace() const { return m_hoverFace; }
    int selectedFace() const { return m_selectedFace; }
    double fps() const { return m_fps; }
    double gpuMs() const { return m_gpuMs; }
    double pickLatencyMs() const { return m_pickLatencyMs; }
    QString backend() const { return m_backend; }

    Q_INVOKABLE void zoomBy(double factor);

    // --- used by the renderer (render thread, GUI thread blocked during synchronize) ---
    struct Snapshot {
        std::shared_ptr<const Scene> scene;
        QMatrix4x4 view;
        QMatrix4x4 projection;
        int hoverFace = 0;
        int selectedFace = 0;
        bool pickRequested = false;
        QPoint pickPixel;
        qint64 pickRequestedAtNs = 0;
        qint64 pickId = 0;
    };
    Snapshot takeSnapshot(float aspect, const QSize& pixelSize);
    void reportFrame(double gpuSeconds, const QString& backendName);
    void reportPick(qint64 pickId, int faceId, double latencyMs);
    std::chrono::steady_clock::time_point clockOrigin() const { return m_clockOrigin; }

protected:
    QQuickRhiItemRenderer* createRenderer() override;
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
    void finishBenchmark();

    std::shared_ptr<const Scene> m_scene;
    OrbitCamera m_camera;
    QPointF m_lastMouse;
    Qt::MouseButtons m_buttons = Qt::NoButton;
    bool m_dragged = false;

    int m_hoverFace = 0;
    int m_selectedFace = 0;
    bool m_pickWanted = false;
    QPoint m_pickPixel;
    QElapsedTimer m_clock;
    std::chrono::steady_clock::time_point m_clockOrigin;
    qint64 m_pickWantedAtNs = 0;
    qint64 m_nextPickId = 1;

    // statistics
    double m_fps = 0.0;
    double m_gpuMs = 0.0;
    double m_pickLatencyMs = 0.0;
    QString m_backend;
    std::vector<double> m_frameIntervalsMs;
    qint64 m_lastFrameNs = 0;
    qint64 m_lastStatsNs = 0;
    double m_gpuSum = 0.0;
    int m_gpuCount = 0;

    // benchmark
    bool m_benchmark = false;
    qint64 m_benchmarkStartNs = 0;
    std::vector<double> m_benchIntervalsMs;
    double m_benchGpuSum = 0.0;
    int m_benchGpuCount = 0;
    std::vector<double> m_benchPickMs;
    int m_benchFrame = 0;
    int m_screenshotFrames = 0;
};

} // namespace fission::spike
