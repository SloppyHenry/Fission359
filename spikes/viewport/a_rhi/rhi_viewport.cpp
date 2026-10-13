// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rhi_viewport.h"

#include "../common/run_options.h"
#include "mesh_pass.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QSaveFile>
#include <QWheelEvent>
#include <QtCore/qmath.h>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace fission::spike {

namespace {

// Thin adapter: Qt Quick calls initialize / synchronize / render, MeshPass does the work.
class MeshRenderer : public QQuickRhiItemRenderer {
public:
    void initialize(QRhiCommandBuffer* /*cb*/) override {
        if (m_rhi != rhi()) {
            m_rhi = rhi();
            m_pass = std::make_unique<MeshPass>(m_rhi);
            m_pipelinePass = nullptr;
        }
        // The pipelines depend on the render pass of the item's render target.
        if (m_pipelinePass != renderTarget()->renderPassDescriptor() ||
            m_pipelineSamples != renderTarget()->sampleCount()) {
            m_pipelinePass = renderTarget()->renderPassDescriptor();
            m_pipelineSamples = renderTarget()->sampleCount();
            m_pass->createPipelines(m_pipelinePass, m_pipelineSamples);
        }
    }

    void synchronize(QQuickRhiItem* item) override {
        m_item = static_cast<RhiViewport*>(item);
        const QSize size = renderTarget() ? renderTarget()->pixelSize() : QSize(1, 1);
        const float aspect = size.height() > 0 ? float(size.width()) / float(size.height()) : 1.0F;
        m_snapshot = m_item->takeSnapshot(aspect, size);
    }

    void render(QRhiCommandBuffer* cb) override {
        if (!m_snapshot.scene || m_snapshot.scene->mesh.triangleCount() == 0 || !m_pass) {
            return;
        }
        if (m_pass->uploadedScene() != m_snapshot.scene.get()) {
            m_pass->upload(cb, *m_snapshot.scene);
        }
        MeshPass::Frame frame;
        frame.view = m_snapshot.view;
        frame.projection = m_snapshot.projection;
        frame.hoverFace = m_snapshot.hoverFace;
        frame.selectedFace = m_snapshot.selectedFace;

        if (m_snapshot.pickRequested && !m_pass->pickInFlight()) {
            const qint64 pickId = m_snapshot.pickId;
            const qint64 requestedAt = m_snapshot.pickRequestedAtNs;
            const auto origin = m_item->clockOrigin();
            QPointer<RhiViewport> item(m_item);
            m_pass->pick(cb, renderTarget()->pixelSize(), m_snapshot.pickPixel, frame,
                         [item, pickId, requestedAt, origin](int faceId) {
                             const double nowNs = double(
                                 std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now() - origin)
                                     .count());
                             if (item) {
                                 item->reportPick(pickId, faceId,
                                                  (nowNs - double(requestedAt)) / 1.0e6);
                             }
                         });
        }
        m_pass->draw(cb, renderTarget(), frame, QColor::fromRgbF(0.075F, 0.082F, 0.098F, 1.0F));

        double gpuSeconds = -1.0;
        if (m_rhi->isFeatureSupported(QRhi::Timestamps)) {
            gpuSeconds = cb->lastCompletedGpuTime();
        }
        if (m_item) {
            m_item->reportFrame(gpuSeconds, QString::fromUtf8(m_rhi->backendName()));
        }
        update(); // continuous rendering: this is a measurement tool
    }

private:
    QRhi* m_rhi = nullptr;
    RhiViewport* m_item = nullptr;
    RhiViewport::Snapshot m_snapshot;
    std::unique_ptr<MeshPass> m_pass;
    QRhiRenderPassDescriptor* m_pipelinePass = nullptr;
    int m_pipelineSamples = 1;
};

} // namespace

// --- item --------------------------------------------------------------------------------

RhiViewport::RhiViewport(QQuickItem* parent) : QQuickRhiItem(parent) {
    const RunOptions& options = runOptions();
    setSampleCount(options.samples);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    m_clock.start();
    m_clockOrigin = std::chrono::steady_clock::now();

    QElapsedTimer loadTimer;
    loadTimer.start();
    auto scene = std::make_shared<Scene>(makeScene(options.scene.toStdString()));
    if (scene->faceCount == 0) {
        scene = std::make_shared<Scene>(makeScene("part"));
    }
    qInfo("scene %s: %zu triangles, %d faces (built in %.1f s)", scene->name.c_str(),
          scene->mesh.triangleCount(), scene->faceCount, loadTimer.elapsed() / 1000.0);
    m_scene = scene;
    const auto& lo = scene->mesh.boundsMin;
    const auto& hi = scene->mesh.boundsMax;
    m_camera.frame(QVector3D(lo[0], lo[1], lo[2]), QVector3D(hi[0], hi[1], hi[2]));

    if (options.benchmarkSeconds > 0.0) {
        m_benchmark = true;
    }
}

QQuickRhiItemRenderer* RhiViewport::createRenderer() {
    return new MeshRenderer;
}

void RhiViewport::zoomBy(double factor) {
    m_camera.zoom(static_cast<float>(factor));
    update();
}

void RhiViewport::mousePressEvent(QMouseEvent* event) {
    m_lastMouse = event->position();
    m_buttons = event->buttons();
    m_dragged = false;
    event->accept();
}

void RhiViewport::mouseMoveEvent(QMouseEvent* event) {
    const QPointF delta = event->position() - m_lastMouse;
    m_lastMouse = event->position();
    const bool alt = event->modifiers().testFlag(Qt::AltModifier);
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (event->buttons().testFlag(Qt::RightButton) ||
        (event->buttons().testFlag(Qt::LeftButton) && alt)) {
        m_camera.orbit(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        m_dragged = true;
    } else if (event->buttons().testFlag(Qt::MiddleButton) ||
               (event->buttons().testFlag(Qt::LeftButton) && shift)) {
        m_camera.pan(static_cast<float>(delta.x()), static_cast<float>(delta.y()),
                     static_cast<float>(height()));
        m_dragged = true;
    }
    m_pickWanted = true;
    m_pickPixel = event->position().toPoint();
    m_pickWantedAtNs = m_clock.nsecsElapsed();
    update();
}

void RhiViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_dragged && event->button() == Qt::LeftButton) {
        m_selectedFace = (m_selectedFace == m_hoverFace) ? 0 : m_hoverFace;
        emit selectedFaceChanged();
    }
    m_buttons = event->buttons();
    update();
}

void RhiViewport::wheelEvent(QWheelEvent* event) {
    const QPoint pixels = event->pixelDelta();
    if (!pixels.isNull()) {
        // Trackpad: two fingers rotate, with Shift they move the view.
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_camera.pan(static_cast<float>(pixels.x()), static_cast<float>(pixels.y()),
                         static_cast<float>(height()));
        } else {
            m_camera.orbit(static_cast<float>(pixels.x()) * 0.6F,
                           static_cast<float>(pixels.y()) * 0.6F);
        }
    } else {
        m_camera.zoom(event->angleDelta().y() > 0 ? 0.9F : 1.0F / 0.9F);
    }
    event->accept();
    update();
}

void RhiViewport::hoverMoveEvent(QHoverEvent* event) {
    m_pickWanted = true;
    m_pickPixel = event->position().toPoint();
    m_pickWantedAtNs = m_clock.nsecsElapsed();
    update();
}

RhiViewport::Snapshot RhiViewport::takeSnapshot(float aspect, const QSize& pixelSize) {
    const qint64 now = m_clock.nsecsElapsed();

    // frame statistics (one call per rendered frame)
    if (m_lastFrameNs != 0) {
        const double intervalMs = double(now - m_lastFrameNs) / 1.0e6;
        m_frameIntervalsMs.push_back(intervalMs);
        if (m_benchmark) {
            m_benchIntervalsMs.push_back(intervalMs);
        }
    }
    m_lastFrameNs = now;
    if (now - m_lastStatsNs > 500'000'000 && !m_frameIntervalsMs.empty()) {
        double sum = 0.0;
        for (const double value : m_frameIntervalsMs) {
            sum += value;
        }
        m_fps = 1000.0 / (sum / double(m_frameIntervalsMs.size()));
        m_frameIntervalsMs.clear();
        m_lastStatsNs = now;
        if (m_gpuCount > 0) {
            m_gpuMs = m_gpuSum / m_gpuCount * 1000.0;
            m_gpuSum = 0.0;
            m_gpuCount = 0;
        }
        QMetaObject::invokeMethod(this, [this] { emit statsChanged(); }, Qt::QueuedConnection);
    }

    if (m_benchmark) {
        if (m_benchmarkStartNs == 0) {
            m_benchmarkStartNs = now;
        }
        m_camera.orbit(2.0F, 0.0F);
        // Synthetic hover movement: a new pixel every third frame.
        if (++m_benchFrame % 3 == 0 && pixelSize.width() > 0) {
            m_pickWanted = true;
            m_pickPixel = QPoint((m_benchFrame * 97) % pixelSize.width(),
                                 (m_benchFrame * 53) % pixelSize.height());
            m_pickWantedAtNs = now;
        }
        if (double(now - m_benchmarkStartNs) / 1.0e9 >= runOptions().benchmarkSeconds) {
            m_benchmark = false;
            QMetaObject::invokeMethod(this, [this] { finishBenchmark(); }, Qt::QueuedConnection);
        }
    }

    if (!runOptions().screenshot.isEmpty() && ++m_screenshotFrames == 30) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                const QImage image = window()->grabWindow();
                image.save(runOptions().screenshot);
                QCoreApplication::quit();
            },
            Qt::QueuedConnection);
    }

    Snapshot snapshot;
    snapshot.scene = m_scene;
    snapshot.view = m_camera.view();
    snapshot.projection = m_camera.projection(aspect);
    snapshot.hoverFace = m_hoverFace;
    snapshot.selectedFace = m_selectedFace;
    if (m_pickWanted) {
        // item (logical) pixels -> render target (device) pixels
        const double scaleX = width() > 0 ? double(pixelSize.width()) / width() : 1.0;
        const double scaleY = height() > 0 ? double(pixelSize.height()) / height() : 1.0;
        snapshot.pickRequested = true;
        snapshot.pickPixel = QPoint(int(m_pickPixel.x() * scaleX), int(m_pickPixel.y() * scaleY));
        snapshot.pickPixel.setX(std::clamp(snapshot.pickPixel.x(), 0, pixelSize.width() - 1));
        snapshot.pickPixel.setY(std::clamp(snapshot.pickPixel.y(), 0, pixelSize.height() - 1));
        snapshot.pickRequestedAtNs = m_pickWantedAtNs;
        snapshot.pickId = m_nextPickId++;
        m_pickWanted = false;
    }
    return snapshot;
}

void RhiViewport::reportFrame(double gpuSeconds, const QString& backendName) {
    if (gpuSeconds > 0.0) {
        m_gpuSum += gpuSeconds;
        ++m_gpuCount;
        if (m_benchmark) {
            m_benchGpuSum += gpuSeconds;
            ++m_benchGpuCount;
        }
    }
    if (m_backend != backendName) {
        m_backend = backendName;
    }
}

void RhiViewport::reportPick(qint64 /*pickId*/, int faceId, double latencyMs) {
    QMetaObject::invokeMethod(
        this,
        [this, faceId, latencyMs] {
            if (faceId != m_hoverFace) {
                m_hoverFace = faceId;
                emit hoverFaceChanged();
            }
            m_pickLatencyMs = 0.9 * m_pickLatencyMs + 0.1 * latencyMs;
            m_benchPickMs.push_back(latencyMs);
            update();
        },
        Qt::QueuedConnection);
}

void RhiViewport::finishBenchmark() {
    auto percentile = [](std::vector<double> values, double fraction) {
        if (values.empty()) {
            return 0.0;
        }
        std::sort(values.begin(), values.end());
        return values[std::min(values.size() - 1, std::size_t(fraction * double(values.size())))];
    };
    double sum = 0.0;
    for (const double value : m_benchIntervalsMs) {
        sum += value;
    }
    const double averageMs = m_benchIntervalsMs.empty() ? 0.0 : sum / double(m_benchIntervalsMs.size());
    double pickSum = 0.0;
    for (const double value : m_benchPickMs) {
        pickSum += value;
    }
    const double pickAverage = m_benchPickMs.empty() ? 0.0 : pickSum / double(m_benchPickMs.size());
    const double gpuAverage = m_benchGpuCount > 0 ? m_benchGpuSum / m_benchGpuCount * 1000.0 : -1.0;

    const QString json = QStringLiteral(
        "{\"prototype\":\"A-rhi\",\"backend\":\"%1\",\"scene\":\"%2\",\"triangles\":%3,"
        "\"faces\":%4,\"samples\":%5,\"vsync\":%6,\"frames\":%7,\"fps_avg\":%8,"
        "\"frame_ms_p50\":%9,\"frame_ms_p95\":%10,\"frame_ms_p99\":%11,\"gpu_ms_avg\":%12,"
        "\"pick_ms_avg\":%13,\"pick_ms_p95\":%14,\"picks\":%15}")
                             .arg(m_backend, QString::fromStdString(m_scene->name))
                             .arg(triangles())
                             .arg(faceCount())
                             .arg(runOptions().samples)
                             .arg(runOptions().vsync ? "true" : "false")
                             .arg(m_benchIntervalsMs.size())
                             .arg(averageMs > 0 ? 1000.0 / averageMs : 0.0, 0, 'f', 1)
                             .arg(percentile(m_benchIntervalsMs, 0.5), 0, 'f', 2)
                             .arg(percentile(m_benchIntervalsMs, 0.95), 0, 'f', 2)
                             .arg(percentile(m_benchIntervalsMs, 0.99), 0, 'f', 2)
                             .arg(gpuAverage, 0, 'f', 2)
                             .arg(pickAverage, 0, 'f', 2)
                             .arg(percentile(m_benchPickMs, 0.95), 0, 'f', 2)
                             .arg(m_benchPickMs.size());
    std::printf("BENCH %s\n", qPrintable(json));
    std::fflush(stdout);
    if (!runOptions().resultFile.isEmpty()) {
        QFile file(runOptions().resultFile);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.write(json.toUtf8() + "\n");
        }
    }
    QCoreApplication::quit();
}

} // namespace fission::spike
