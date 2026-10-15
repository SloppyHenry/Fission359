// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/viewport_3d.h"

#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "fission/render/detail.h"
#include "fission/render/scene_renderer.h"

#include <QtQml/qqml.h>
#include <rhi/qrhi.h>

namespace fission::ui {

namespace {

constexpr int kAnimationMilliseconds = 180;
constexpr float kWheelStep = 0.9F;     // one notch of a mouse wheel zooms by 10 %
constexpr float kTrackpadOrbit = 0.6F; // two-finger drag: pixels to orbit pixels
constexpr double kFrameBudgetMilliseconds = 16.7; // 60 frames per second
constexpr int kSlowFramesToSwitchOff = 90;
constexpr double kDragZoomPerPixel = 0.01; // Ctrl + middle button (Blender): drag up zooms in

// Thin adapter: Qt Quick calls initialize / synchronize / render, SceneRenderer does the work.
class SceneItemRenderer : public QQuickRhiItemRenderer {
public:
    void initialize(QRhiCommandBuffer* /*cb*/) override {
        if (m_rhi != rhi()) {
            m_rhi = rhi();
            m_renderer = std::make_unique<render::SceneRenderer>(m_rhi);
            m_pass = nullptr;
            m_uploadedScene = 0;
            m_uploadedPalette = 0;
        }
        // Pipelines depend on the render pass of the item's render target.
        if (m_pass != renderTarget()->renderPassDescriptor() ||
            m_samples != renderTarget()->sampleCount()) {
            m_pass = renderTarget()->renderPassDescriptor();
            m_samples = renderTarget()->sampleCount();
            m_renderer->createPipelines(m_pass, m_samples);
        }
    }

    void synchronize(QQuickRhiItem* item) override {
        m_item = static_cast<Viewport3D*>(item);
        const QSize size = renderTarget() ? renderTarget()->pixelSize() : QSize(1, 1);
        const float aspect = size.height() > 0 ? float(size.width()) / float(size.height()) : 1.0F;
        m_snapshot = m_item->takeSnapshot(aspect);
    }

    void render(QRhiCommandBuffer* cb) override {
        if (!m_renderer || !m_snapshot.scene) {
            return;
        }
        if (m_uploadedScene != m_snapshot.sceneVersion ||
            m_uploadedPalette != m_snapshot.paletteVersion) {
            m_renderer->upload(cb, *m_snapshot.scene, m_snapshot.palette);
            m_uploadedScene = m_snapshot.sceneVersion;
            m_uploadedPalette = m_snapshot.paletteVersion;
        }
        render::SceneRenderer::Frame frame;
        frame.view = m_snapshot.view;
        frame.projection = m_snapshot.projection;
        frame.pixelsPerMillimetre = m_snapshot.pixelsPerMillimetre;
        frame.bedVisible = m_snapshot.bedVisible;
        frame.displayMode = m_snapshot.displayMode;
        frame.palette = m_snapshot.palette;
        frame.ambientOcclusion = m_snapshot.ambientOcclusion;
        const auto started = std::chrono::steady_clock::now();
        m_renderer->draw(cb, renderTarget(), frame);
        const double cpuMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
        const double gpuMilliseconds = m_rhi->isFeatureSupported(QRhi::Timestamps)
                                           ? cb->lastCompletedGpuTime() * 1000.0
                                           : 0.0;
        if (m_item != nullptr) {
            m_item->recordFrame(gpuMilliseconds, cpuMilliseconds);
        }
        watchFrameTime(cb);
        if (m_item != nullptr) {
            m_item->reportRendered(QString::fromUtf8(m_rhi->backendName()));
        }
    }

private:
    // The graphics card says how long it took for the last finished frame (where it can).
    // 90 frames in a row above the budget mean that the soft shadows cost too much.
    void watchFrameTime(QRhiCommandBuffer* cb) {
        if (m_item == nullptr || !m_snapshot.ambientOcclusion ||
            !m_rhi->isFeatureSupported(QRhi::Timestamps)) {
            m_slowFrames = 0;
            m_sumMilliseconds = 0.0;
            return;
        }
        const double milliseconds = cb->lastCompletedGpuTime() * 1000.0;
        if (milliseconds <= 0.0) {
            return;
        }
        if (milliseconds > kFrameBudgetMilliseconds) {
            ++m_slowFrames;
            m_sumMilliseconds += milliseconds;
            if (m_slowFrames >= kSlowFramesToSwitchOff) {
                m_item->reportSlowFrames(m_sumMilliseconds / m_slowFrames);
                m_slowFrames = 0;
                m_sumMilliseconds = 0.0;
            }
        } else {
            m_slowFrames = 0;
            m_sumMilliseconds = 0.0;
        }
    }

    int m_slowFrames = 0;
    double m_sumMilliseconds = 0.0;
    QRhi* m_rhi = nullptr;
    Viewport3D* m_item = nullptr;
    Viewport3D::Snapshot m_snapshot;
    std::unique_ptr<render::SceneRenderer> m_renderer;
    QRhiRenderPassDescriptor* m_pass = nullptr;
    int m_samples = 1;
    quint64 m_uploadedScene = 0;
    quint64 m_uploadedPalette = 0;
};

} // namespace

Viewport3D::Viewport3D(QQuickItem* parent) : QQuickRhiItem(parent) {
    setSampleCount(4); // smooth edges
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemAcceptsInputMethod, false);

    m_scene = std::make_shared<render::RenderScene>();
    m_shown = m_scene;
    const render::BedSpec bed = m_scene->bed;
    m_camera.setViewportSize(QSizeF(800.0, 600.0));
    m_camera.frame({-0.5F * bed.sizeX, -0.5F * bed.sizeY, 0.0F},
                   {0.5F * bed.sizeX, 0.5F * bed.sizeY, 0.0F});

    m_animation.setDuration(kAnimationMilliseconds);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    m_animation.setStartValue(0.0);
    m_animation.setEndValue(1.0);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_camera.setPose(
            render::blend(m_animationFrom, m_animationTo, static_cast<float>(value.toDouble())));
        cameraMoved();
    });

    connect(this, &Viewport3D::paletteChanged, this, [this] {
        ++m_paletteVersion;
        update();
    });
}

QQuickRhiItemRenderer* Viewport3D::createRenderer() {
    return new SceneItemRenderer;
}

render::ViewPalette Viewport3D::palette() const {
    render::ViewPalette palette;
    palette.viewCenter = m_viewCenter;
    palette.viewEdge = m_viewEdge;
    palette.bed = m_bed;
    palette.bedEdge = m_bedEdge;
    palette.grid = m_grid;
    palette.gridMajor = m_gridMajor;
    palette.axisX = m_axisX;
    palette.axisY = m_axisY;
    palette.origin = m_origin;
    palette.edgeLine = m_edgeLine;
    palette.shadow = m_shadow;
    return palette;
}

void Viewport3D::setScene(std::shared_ptr<const render::RenderScene> scene) {
    m_scene = std::move(scene);
    m_bodyVisible.assign(m_scene ? m_scene->bodies.size() : 0, true);
    rebuildVisibleScene();
    emit sceneChanged();
    update();
}

void Viewport3D::rebuildVisibleScene() {
    if (m_scene &&
        std::find(m_bodyVisible.begin(), m_bodyVisible.end(), false) != m_bodyVisible.end()) {
        auto shown = std::make_shared<render::RenderScene>();
        shown->bed = m_scene->bed;
        for (std::size_t i = 0; i < m_scene->bodies.size(); ++i) {
            if (m_bodyVisible[i]) {
                shown->bodies.push_back(m_scene->bodies[i]);
            }
        }
        m_shown = std::move(shown);
    } else {
        m_shown = m_scene;
    }
    ++m_sceneVersion;
}

void Viewport3D::setBodyVisible(int index, bool visible) {
    if (index < 0 || static_cast<std::size_t>(index) >= m_bodyVisible.size() ||
        m_bodyVisible[static_cast<std::size_t>(index)] == visible) {
        return;
    }
    m_bodyVisible[static_cast<std::size_t>(index)] = visible;
    rebuildVisibleScene();
    update();
}

void Viewport3D::setDisplayMode(const QString& mode) {
    if (m_displayMode != mode && (mode == QLatin1String("wire") || mode == QLatin1String("solid") ||
                                  mode == QLatin1String("color"))) {
        m_displayMode = mode;
        emit displayModeChanged();
        update();
    }
}

int Viewport3D::triangleCount() const {
    int count = 0;
    if (m_scene) {
        for (const render::RenderBody& body : m_scene->bodies) {
            count += static_cast<int>(body.geometry().triangleCount());
        }
    }
    return count;
}

double Viewport3D::bedSizeX() const {
    return m_scene ? static_cast<double>(m_scene->bed.sizeX) : 0.0;
}

double Viewport3D::bedSizeY() const {
    return m_scene ? static_cast<double>(m_scene->bed.sizeY) : 0.0;
}

void Viewport3D::setBedVisible(bool visible) {
    if (m_bedVisible != visible) {
        m_bedVisible = visible;
        emit bedVisibleChanged();
        update();
    }
}

void Viewport3D::setOrthographic(bool orthographic) {
    if (m_camera.orthographic() != orthographic) {
        m_camera.setOrthographic(orthographic);
        emit orthographicChanged();
        cameraMoved();
    }
}

void Viewport3D::setAmbientOcclusion(bool on) {
    if (m_ambientOcclusion != on) {
        m_ambientOcclusion = on;
        emit ambientOcclusionChanged();
        update();
    }
}

void Viewport3D::startFrameStatistics() {
    const std::lock_guard lock(m_statisticsMutex);
    m_gpuTimes.clear();
    m_cpuTimes.clear();
    m_collecting = true;
}

Viewport3D::FrameStatistics Viewport3D::finishFrameStatistics() {
    const std::lock_guard lock(m_statisticsMutex);
    m_collecting = false;
    FrameStatistics result;
    result.frames = static_cast<int>(m_cpuTimes.size());
    const auto average = [](const std::vector<double>& values) {
        double sum = 0.0;
        for (const double v : values) {
            sum += v;
        }
        return values.empty() ? 0.0 : sum / static_cast<double>(values.size());
    };
    result.cpuAverageMs = average(m_cpuTimes);
    result.gpuAverageMs = average(m_gpuTimes);
    if (!m_gpuTimes.empty()) {
        std::vector<double> sorted = m_gpuTimes;
        std::sort(sorted.begin(), sorted.end());
        result.gpuP95Ms = sorted[static_cast<std::size_t>(0.95 * static_cast<double>(sorted.size() - 1))];
    }
    return result;
}

void Viewport3D::recordFrame(double gpuMilliseconds, double cpuMilliseconds) {
    const std::lock_guard lock(m_statisticsMutex);
    if (!m_collecting) {
        return;
    }
    m_cpuTimes.push_back(cpuMilliseconds);
    if (gpuMilliseconds > 0.0) {
        m_gpuTimes.push_back(gpuMilliseconds);
    }
}

void Viewport3D::reportSlowFrames(double averageMilliseconds) {
    // Called on the render thread: continue on the interface thread.
    QMetaObject::invokeMethod(
        this,
        [this, averageMilliseconds] {
            if (m_ambientOcclusion) {
                setAmbientOcclusion(false);
                emit ambientOcclusionTurnedOff(averageMilliseconds);
            }
        },
        Qt::QueuedConnection);
}

void Viewport3D::cameraMoved() {
    const int level = render::detailLevelFor(m_camera.pixelsPerMillimetre(), m_detailLevel);
    if (level != m_detailLevel) {
        m_detailLevel = level;
        emit detailLevelWanted(level);
    }
    emit cameraChanged();
    update();
}

void Viewport3D::setStandardView(const QString& name) {
    using render::StandardView;
    static const struct {
        const char* name;
        StandardView view;
    } views[] = {{"home", StandardView::home},     {"top", StandardView::top},
                 {"bottom", StandardView::bottom}, {"front", StandardView::front},
                 {"back", StandardView::back},     {"left", StandardView::left},
                 {"right", StandardView::right}};
    for (const auto& entry : views) {
        if (name == QLatin1String(entry.name)) {
            animateTo(m_camera.poseFor(entry.view));
            return;
        }
    }
}

void Viewport3D::setCamera(double yaw, double pitch, double distance) {
    m_animation.stop();
    render::CameraPose pose = m_camera.pose();
    pose.yaw = static_cast<float>(yaw);
    pose.pitch = static_cast<float>(pitch);
    pose.distance = static_cast<float>(distance);
    m_camera.setPose(pose);
    cameraMoved();
}

void Viewport3D::frameAll() {
    const render::BedSpec bed = m_scene ? m_scene->bed : render::BedSpec{};
    render::OrbitCamera target = m_camera;
    target.frame({-0.5F * bed.sizeX, -0.5F * bed.sizeY, 0.0F},
                 {0.5F * bed.sizeX, 0.5F * bed.sizeY, 40.0F});
    render::CameraPose pose = target.pose();
    pose.yaw = m_camera.pose().yaw;
    pose.pitch = m_camera.pose().pitch;
    animateTo(pose);
}

void Viewport3D::animateTo(const render::CameraPose& target) {
    m_animation.stop();
    if (!m_animated || !isVisible()) {
        m_camera.setPose(target);
        cameraMoved();
        return;
    }
    m_animationFrom = m_camera.pose();
    m_animationTo = target;
    m_animation.start();
}

void Viewport3D::zoomBy(double factor, double x, double y) {
    m_animation.stop();
    if (x < 0.0 || y < 0.0) {
        m_camera.zoomAt(static_cast<float>(factor));
    } else {
        m_camera.zoomAt(static_cast<float>(factor), QPointF(x, y));
    }
    cameraMoved();
}

void Viewport3D::panBy(double dx, double dy) {
    m_animation.stop();
    m_camera.pan(static_cast<float>(dx), static_cast<float>(dy));
    cameraMoved();
}

void Viewport3D::orbitBy(double dx, double dy) {
    m_animation.stop();
    m_camera.orbit(static_cast<float>(dx), static_cast<float>(dy));
    cameraMoved();
}

QPointF Viewport3D::project(double x, double y, double z) const {
    return m_camera.project(
        QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}

void Viewport3D::mousePressEvent(QMouseEvent* event) {
    m_lastMouse = event->position();
    m_animation.stop();
    event->accept();
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    const QPointF delta = event->position() - m_lastMouse;
    m_lastMouse = event->position();
    applyNavigation(render::dragAction(m_mouseStyle, event->buttons(), event->modifiers()),
                    delta.x(), delta.y());
    event->accept();
}

void Viewport3D::mouseDoubleClickEvent(QMouseEvent* event) {
    // A double click with the left button on a face turns the view to look at it squarely.
    // (AP-1.4 may give the double click to the selection; the view then keeps this on a key.)
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier) {
        lookAtSurface(event->position().x(), event->position().y());
    }
    event->accept();
}

bool Viewport3D::event(QEvent* event) {
    if (event->type() == QEvent::NativeGesture) {
        const auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            // Spreading the fingers is a positive value and brings the scene closer.
            const double factor = 1.0 / std::max(1.0 + gesture->value(), 0.05);
            zoomBy(factor, gesture->position().x(), gesture->position().y());
            event->accept();
            return true;
        }
        if (gesture->gestureType() == Qt::SmartZoomNativeGesture) {
            zoomToSurface(gesture->position().x(), gesture->position().y());
            event->accept();
            return true;
        }
    }
    return QQuickRhiItem::event(event);
}

void Viewport3D::wheelEvent(QWheelEvent* event) {
    const QPoint pixels = event->pixelDelta();
    if (!pixels.isNull()) {
        // Trackpad: two fingers turn the view, with Shift they move it.
        const render::NavigationAction action =
            render::trackpadAction(m_mouseStyle, event->modifiers());
        const double scale = action == render::NavigationAction::orbit
                                 ? static_cast<double>(kTrackpadOrbit)
                                 : 1.0;
        applyNavigation(action, pixels.x() * scale, pixels.y() * scale);
    } else if (event->angleDelta().y() != 0) {
        // Mouse wheel: zoom towards the pointer; 120 units are one notch.
        const double notches = event->angleDelta().y() / 120.0;
        const double factor = std::pow(static_cast<double>(kWheelStep), notches);
        zoomBy(factor, event->position().x(), event->position().y());
    }
    event->accept();
}

void Viewport3D::applyNavigation(render::NavigationAction action, double dx, double dy) {
    switch (action) {
    case render::NavigationAction::orbit:
        orbitBy(dx, dy);
        break;
    case render::NavigationAction::pan:
        panBy(dx, dy);
        break;
    case render::NavigationAction::zoom:
        // Dragging up (dy < 0) comes closer.
        zoomBy(std::exp(dy * kDragZoomPerPixel));
        break;
    case render::NavigationAction::none:
        break;
    }
}

QString Viewport3D::mouseStyle() const {
    return QString::fromUtf8(render::mouseStyleName(m_mouseStyle).data(),
                             static_cast<qsizetype>(render::mouseStyleName(m_mouseStyle).size()));
}

void Viewport3D::setMouseStyle(const QString& name) {
    const std::optional<render::MouseStyle> style =
        render::mouseStyleFromName(name.toStdString());
    if (style && *style != m_mouseStyle) {
        m_mouseStyle = *style;
        emit mouseStyleChanged();
    }
}

void Viewport3D::setViewDirection(double x, double y, double z) {
    animateTo(m_camera.poseLookingFrom(
        QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))));
}

std::optional<render::SurfaceHit> Viewport3D::surfaceAt(double x, double y) const {
    if (!m_shown) {
        return std::nullopt;
    }
    const std::optional<render::Ray> ray = m_camera.rayThrough(QPointF(x, y));
    if (!ray) {
        return std::nullopt;
    }
    return render::pickSurface(*m_shown, *ray);
}

bool Viewport3D::lookAtSurface(double x, double y) {
    const std::optional<render::SurfaceHit> hit = surfaceAt(x, y);
    if (!hit) {
        return false;
    }
    QVector3D center = hit->point;
    if (hit->bodyIndex >= 0) {
        if (const auto shape = render::faceShape(
                m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].geometry(), hit->faceId)) {
            center = shape->center;
        }
    }
    render::CameraPose pose = m_camera.poseLookingFrom(hit->normal);
    pose.target = center;
    animateTo(pose);
    return true;
}

bool Viewport3D::zoomToSurface(double x, double y) {
    const std::optional<render::SurfaceHit> hit = surfaceAt(x, y);
    if (!hit) {
        return false;
    }
    if (hit->bodyIndex >= 0) {
        if (const auto shape = render::faceShape(
                m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].geometry(), hit->faceId)) {
            animateTo(m_camera.poseFraming(shape->center, shape->radius));
            return true;
        }
    }
    render::CameraPose pose = m_camera.pose();
    pose.target = hit->point;
    animateTo(pose);
    return true;
}

void Viewport3D::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickRhiItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size() && newGeometry.width() > 0.0 &&
        newGeometry.height() > 0.0) {
        m_camera.setViewportSize(newGeometry.size());
        cameraMoved();
    }
}

Viewport3D::Snapshot Viewport3D::takeSnapshot(float aspect) {
    Snapshot snapshot;
    snapshot.scene = m_shown ? m_shown : m_scene;
    snapshot.displayMode = m_displayMode == QLatin1String("wire") ? render::DisplayMode::wireframe
                           : m_displayMode == QLatin1String("solid") ? render::DisplayMode::solid
                                                                     : render::DisplayMode::colors;
    snapshot.palette = palette();
    snapshot.view = m_camera.view();
    snapshot.projection = m_camera.projection(aspect);
    snapshot.pixelsPerMillimetre = m_camera.pixelsPerMillimetre();
    snapshot.bedVisible = m_bedVisible;
    snapshot.ambientOcclusion = m_ambientOcclusion;
    snapshot.sceneVersion = m_sceneVersion;
    snapshot.paletteVersion = m_paletteVersion;
    return snapshot;
}

void Viewport3D::reportRendered(const QString& backendName) {
    // Called on the render thread: hand over to the GUI thread.
    QMetaObject::invokeMethod(
        this,
        [this, backendName] {
            if (m_backendName != backendName) {
                m_backendName = backendName;
                emit backendNameChanged();
            }
            if (!m_firstFrameReported) {
                m_firstFrameReported = true;
                if (QQuickWindow* w = window()) {
                    // "Shown" means the window has swapped once after our first render.
                    auto connection = std::make_shared<QMetaObject::Connection>();
                    *connection = connect(w, &QQuickWindow::frameSwapped, this, [this, connection] {
                        disconnect(*connection);
                        emit firstFrameShown();
                    });
                } else {
                    emit firstFrameShown();
                }
            }
        },
        Qt::QueuedConnection);
}

void registerViewportTypes() {
    qmlRegisterType<Viewport3D>("Fission3D.View", 1, 0, "Viewport3D");
}

} // namespace fission::ui
