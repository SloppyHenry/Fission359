// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/viewport_3d.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "fission/interaction/drag.h"
#include "fission/render/detail.h"
#include "fission/render/scene_renderer.h"

#include <QtQml/qqml.h>
#include <rhi/qrhi.h>

namespace fission::ui {

namespace {

constexpr int kAnimationMilliseconds = 180;
constexpr float kWheelStep = 0.9F;                // one notch of a mouse wheel zooms by 10 %
constexpr float kTrackpadOrbit = 0.6F;            // two-finger drag: pixels to orbit pixels
constexpr double kFrameBudgetMilliseconds = 16.7; // 60 frames per second
constexpr int kSlowFramesToSwitchOff = 90;
constexpr double kDragZoomPerPixel = 0.01; // Ctrl + middle button (Blender): drag up zooms in
constexpr double kClickReach = 4.0;        // pixels the pointer may move and still click

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
        m_snapshot = m_item->takeSnapshot(aspect, m_renderer && !m_renderer->pickInFlight());
    }

    void render(QRhiCommandBuffer* cb) override {
        if (!m_renderer || !m_snapshot.scene) {
            answerNothing();
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
        frame.snapGridStep = m_snapshot.snapGridStep;
        frame.displayMode = m_snapshot.displayMode;
        frame.palette = m_snapshot.palette;
        frame.ambientOcclusion = m_snapshot.ambientOcclusion;
        frame.highlights = m_snapshot.highlights;
        frame.handles = m_snapshot.handles;
        frame.accent = m_snapshot.accent;
        const auto started = std::chrono::steady_clock::now();
        m_renderer->draw(cb, renderTarget(), frame);
        const double cpuMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
        const double gpuMilliseconds =
            m_rhi->isFeatureSupported(QRhi::Timestamps) ? cb->lastCompletedGpuTime() * 1000.0 : 0.0;
        if (m_item != nullptr) {
            m_item->recordFrame(gpuMilliseconds, cpuMilliseconds);
        }
        watchFrameTime(cb);
        startPick(cb);
        if (m_item != nullptr) {
            m_item->reportRendered(QString::fromUtf8(m_rhi->backendName()));
        }
    }

private:
    // The answer "nothing" for a request that could not be drawn.
    void answerNothing() {
        if (m_snapshot.pick && m_item != nullptr) {
            Viewport3D::PickResult none;
            none.request = *m_snapshot.pick;
            m_item->reportPicked(std::move(none));
            m_snapshot.pick.reset();
        }
    }

    // Draws the pick picture for the request of this frame. The graphics card finishes it
    // during this or a later frame; the answer goes to the interface thread from there.
    void startPick(QRhiCommandBuffer* cb) {
        if (!m_snapshot.pick || m_item == nullptr) {
            return;
        }
        const Viewport3D::PickRequest request = *m_snapshot.pick;
        m_snapshot.pick.reset();
        const QSize size = renderTarget()->pixelSize();
        const double ratio =
            m_snapshot.itemSize.width() > 0.0 ? size.width() / m_snapshot.itemSize.width() : 1.0;
        const QPoint pixel(static_cast<int>(std::floor(request.position.x() * ratio)),
                           static_cast<int>(std::floor(request.position.y() * ratio)));
        const QPointer<Viewport3D> item = m_item;
        render::SceneRenderer* renderer = m_renderer.get();
        if (request.intent == Viewport3D::PickIntent::region) {
            const QRect area(static_cast<int>(std::floor(request.area.left() * ratio)),
                             static_cast<int>(std::floor(request.area.top() * ratio)),
                             static_cast<int>(std::ceil(request.area.width() * ratio)),
                             static_cast<int>(std::ceil(request.area.height() * ratio)));
            const bool regionStarted = renderer->pickRegion(
                cb, size, area, m_snapshot.view, m_snapshot.projection, request.filter,
                [item, request, renderer](std::vector<std::uint32_t> picture) {
                    Viewport3D::PickResult result;
                    result.request = request;
                    for (const auto& found : render::chooseInRegion(picture, request.filter)) {
                        const std::string body = renderer->bodyOfSlot(found.slot);
                        if (!body.empty()) {
                            result.elements.push_back(
                                {QString::fromStdString(body), found.kind, found.number});
                        }
                    }
                    if (item) {
                        item->reportPicked(std::move(result));
                    }
                });
            if (regionStarted) {
                update();
            } else {
                Viewport3D::PickResult none;
                none.request = request;
                m_item->reportPicked(std::move(none));
            }
            return;
        }
        const bool started = renderer->pick(
            cb, size, pixel, m_snapshot.view, m_snapshot.projection, request.filter,
            [item, request, renderer](std::vector<std::uint32_t> picture) {
                Viewport3D::PickResult result;
                result.request = request;
                result.workMilliseconds = renderer->lastPickMilliseconds();
                if (const auto found = render::choosePicked(picture, request.filter)) {
                    result.body = QString::fromStdString(renderer->bodyOfSlot(found->slot));
                    result.kind = found->kind;
                    result.number = found->number;
                }
                if (item) {
                    item->reportPicked(std::move(result));
                }
            });
        if (started) {
            update(); // one more frame, so that the graphics card's answer is collected
        } else {
            Viewport3D::PickResult none;
            none.request = request;
            m_item->reportPicked(std::move(none));
        }
    }

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
    setAcceptHoverEvents(true);
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

    // The graphics card hands over the pick picture with a later frame. While a question is
    // open, frames are asked for, so that the answer does not wait for the next mouse move.
    m_pickTimer.setInterval(8);
    connect(&m_pickTimer, &QTimer::timeout, this, [this] {
        if (m_pickQueue.empty() && m_picksInFlight.load() == 0) {
            m_pickTimer.stop();
        } else {
            update();
        }
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
    hoverAgain(); // what is under the pointer may be another thing now
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

double Viewport3D::snapStep() const {
    return snapStepFor(m_camera.pixelsPerMillimetre());
}

double Viewport3D::snapStepFor(double pixelsPerMillimetre) const {
    return m_fixedSnapStep > 0.0 ? m_fixedSnapStep : interaction::snapStep(pixelsPerMillimetre);
}

void Viewport3D::setFixedSnapStep(double millimetres) {
    // Anything that is not a step from 0.001 mm up is "automatic".
    const double step = std::isfinite(millimetres) && millimetres >= 0.001 ? millimetres : 0.0;
    if (step != m_fixedSnapStep) {
        m_fixedSnapStep = step;
        emit snapStepChanged();
    }
}

void Viewport3D::setSnapGridShown(bool shown) {
    if (m_snapGridShown != shown) {
        m_snapGridShown = shown;
        emit snapGridShownChanged();
        update();
    }
}

void Viewport3D::setSnapGridStep(double millimetres) {
    if (millimetres != m_snapGridStep) {
        m_snapGridStep = millimetres;
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
        result.gpuP95Ms =
            sorted[static_cast<std::size_t>(0.95 * static_cast<double>(sorted.size() - 1))];
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
    if (m_fixedSnapStep <= 0.0) {
        emit snapStepChanged(); // the step follows the zoom
    }
    update();
    hoverAgain();
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
    m_pressPosition = event->position();
    m_pressButton = event->button();
    m_pressModifiers = event->modifiers();
    forceActiveFocus(); // a click in the view takes the keyboard from a number field
    m_dragged = false;
    m_rubberBanding = false;
    m_bodyDragging = false;
    m_pressBody.clear();
    if (m_bodiesMovable && event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::NoModifier) {
        if (const auto hit = bodyPointAt(event->position().x(), event->position().y())) {
            m_pressBody = hit->body; // dragged, it moves; clicked, it is chosen as before
        }
    }
    setHover(std::nullopt); // while the view is being moved, nothing lights up
    event->accept();
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    const QPointF delta = event->position() - m_lastMouse;
    m_lastMouse = event->position();
    const render::NavigationAction action =
        render::dragAction(m_mouseStyle, event->buttons(), event->modifiers());
    applyNavigation(action, delta.x(), delta.y());
    if (m_pressButton != Qt::NoButton) {
        const QPointF moved = event->position() - m_pressPosition;
        if (!m_dragged && std::hypot(moved.x(), moved.y()) > kClickReach) {
            m_dragged = true;
        }
        // A drag with the left button that does not move the view moves the body it started
        // on, or else draws a rectangle.
        if (m_dragged && m_pressButton == Qt::LeftButton && event->buttons() == Qt::LeftButton &&
            action == render::NavigationAction::none) {
            if (!m_pressBody.isEmpty() && !m_rubberBanding) {
                if (!m_bodyDragging) {
                    m_bodyDragging = true;
                    emit bodyDragStarted(m_pressBody, m_pressPosition.x(), m_pressPosition.y());
                }
                emit bodyDragMoved(event->position().x(), event->position().y());
            } else {
                m_rubberBanding = true;
                emit rubberBandChanged(QRectF(m_pressPosition, event->position()).normalized());
            }
        }
    }
    event->accept();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != m_pressButton) {
        event->accept();
        return;
    }
    const bool extend = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) != 0;
    if (m_pressButton == Qt::LeftButton && !m_dragged) {
        requestPick(PickIntent::click, event->position(), extend);
    } else if (m_bodyDragging) {
        emit bodyDragFinished();
    } else if (m_rubberBanding) {
        emit rubberBandChanged(QRectF());
        emit rubberBandFinished(QRectF(m_pressPosition, event->position()).normalized(),
                                (event->modifiers() & Qt::ControlModifier) != 0);
    }
    m_pressButton = Qt::NoButton;
    m_rubberBanding = false;
    m_bodyDragging = false;
    m_pressBody.clear();
    m_hoverPosition = event->position();
    hoverAgain();
    event->accept();
}

void Viewport3D::mouseDoubleClickEvent(QMouseEvent* event) {
    // A double click chooses the whole body. (Looking squarely at a face moved to the view
    // cube and the View menu, AP-1.4.)
    if (event->button() == Qt::LeftButton) {
        const bool extend = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) != 0;
        requestPick(PickIntent::doubleClick, event->position(), extend);
    }
    m_pressButton = Qt::NoButton; // the release that follows is not a click
    event->accept();
}

void Viewport3D::hoverMoveEvent(QHoverEvent* event) {
    m_hoverSimulated = false;
    m_hoverPosition = event->position();
    hoverAgain();
    QQuickRhiItem::hoverMoveEvent(event);
}

void Viewport3D::hoverLeaveEvent(QHoverEvent* event) {
    if (m_hoverSimulated) {
        // The pointer that was played by hand (--simulate) stays where it was put, whatever
        // the real pointer does.
        QQuickRhiItem::hoverLeaveEvent(event);
        return;
    }
    m_hoverPosition.reset();
    m_pickQueue.erase(
        std::remove_if(m_pickQueue.begin(), m_pickQueue.end(),
                       [](const PickRequest& r) { return r.intent == PickIntent::hover; }),
        m_pickQueue.end());
    setHover(std::nullopt);
    QQuickRhiItem::hoverLeaveEvent(event);
}

// --- selection --------------------------------------------------------------------------

void Viewport3D::setSelectionFilter(core::SelectionFilter filter) {
    if (!(m_filter == filter)) {
        m_filter = filter;
        setHover(std::nullopt);
        hoverAgain();
    }
}

void Viewport3D::setSelectionHighlights(std::vector<render::Highlight> highlights) {
    m_selectionHighlights = std::move(highlights);
    emit highlightsChanged();
    update();
}

void Viewport3D::setHandleGraphics(render::HandleGraphics graphics, HandleSource source) {
    auto& slot = m_handleGraphics[static_cast<std::size_t>(source)];
    if (graphics == slot) {
        return;
    }
    slot = std::move(graphics);
    update();
}

void Viewport3D::setBodiesMovable(bool movable) {
    if (movable != m_bodiesMovable) {
        m_bodiesMovable = movable;
        emit bodiesMovableChanged();
    }
}

void Viewport3D::requestPick(PickIntent intent, const QPointF& position, bool extend) {
    PickRequest request;
    request.id = m_nextPickId++;
    request.intent = intent;
    request.position = position;
    request.extend = extend;
    request.filter = m_filter;
    if (intent == PickIntent::hover) {
        // Only the newest position matters.
        while (!m_pickQueue.empty() && m_pickQueue.back().intent == PickIntent::hover) {
            m_pickQueue.pop_back();
        }
    }
    m_pickQueue.push_back(request);
    update();
    if (!m_pickTimer.isActive()) {
        m_pickTimer.start();
    }
}

void Viewport3D::hoverAt(double x, double y) {
    m_hoverSimulated = true;
    m_hoverPosition = QPointF(x, y);
    hoverAgain();
}

void Viewport3D::requestRegionPick(const QRectF& area, bool extend) {
    PickRequest request;
    request.id = m_nextPickId++;
    request.intent = PickIntent::region;
    request.area = area;
    request.extend = extend;
    request.filter = m_filter;
    m_pickQueue.push_back(request);
    update();
    if (!m_pickTimer.isActive()) {
        m_pickTimer.start();
    }
}

void Viewport3D::hoverAgain() {
    if (m_hoverPosition && m_pressButton == Qt::NoButton) {
        requestPick(PickIntent::hover, *m_hoverPosition);
    }
}

void Viewport3D::setHover(std::optional<render::Highlight> hover) {
    const auto same = [](const std::optional<render::Highlight>& a,
                         const std::optional<render::Highlight>& b) {
        return a.has_value() == b.has_value() &&
               (!a || (a->body == b->body && a->kind == b->kind && a->number == b->number));
    };
    if (same(m_hover, hover)) {
        return;
    }
    m_hover = std::move(hover);
    if (m_hover) {
        emit hoverChanged(QString::fromStdString(m_hover->body), static_cast<int>(m_hover->kind),
                          static_cast<int>(m_hover->number));
    } else {
        emit hoverChanged(QString(), 0, 0);
    }
    update();
}

void Viewport3D::reportPicked(PickResult result) {
    // Called on the render thread: continue on the interface thread.
    QMetaObject::invokeMethod(
        this, [this, result = std::move(result)] { finishPick(result); }, Qt::QueuedConnection);
}

void Viewport3D::finishPick(const PickResult& result) {
    --m_picksInFlight;
    emit pickTimed(result.workMilliseconds);
    const PickRequest& request = result.request;
    if (request.intent == PickIntent::region) {
        QVariantList elements;
        for (const PickResult::Element& element : result.elements) {
            QVariantMap entry;
            entry[QStringLiteral("body")] = element.body;
            entry[QStringLiteral("kind")] = static_cast<int>(element.kind);
            entry[QStringLiteral("number")] = static_cast<int>(element.number);
            elements.push_back(entry);
        }
        emit regionPicked(elements, request.extend);
        if (!m_pickQueue.empty()) {
            update();
        }
        return;
    }
    if (request.intent == PickIntent::hover) {
        // The pointer may have left or started a drag since the question was asked.
        if (m_hoverPosition && m_pressButton == Qt::NoButton) {
            if (result.hit()) {
                setHover(
                    render::Highlight{result.body.toStdString(), result.kind, result.number, true});
            } else {
                setHover(std::nullopt);
            }
        }
    }
    emit picked(static_cast<int>(request.intent), request.extend, result.body,
                static_cast<int>(result.kind), static_cast<int>(result.number));
    if (!m_pickQueue.empty()) {
        update();
    }
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
        const double scale =
            action == render::NavigationAction::orbit ? static_cast<double>(kTrackpadOrbit) : 1.0;
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
    const std::optional<render::MouseStyle> style = render::mouseStyleFromName(name.toStdString());
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

std::optional<Viewport3D::BodyPoint> Viewport3D::bodyPointAt(double x, double y) const {
    const std::optional<render::SurfaceHit> hit = surfaceAt(x, y);
    if (!hit || hit->bodyIndex < 0 || hit->bodyIndex >= static_cast<int>(m_shown->bodies.size())) {
        return std::nullopt;
    }
    return BodyPoint{
        QString::fromStdString(m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].id),
        hit->point};
}

std::optional<Viewport3D::WorldRay> Viewport3D::rayAt(double x, double y) const {
    const std::optional<render::Ray> ray = m_camera.rayThrough(QPointF(x, y));
    if (!ray) {
        return std::nullopt;
    }
    return WorldRay{ray->origin, ray->direction};
}

std::optional<Viewport3D::Landing> Viewport3D::landingAt(double x, double y,
                                                         const QString& ignoreBody, double planeZ,
                                                         bool onFaces) const {
    const std::optional<render::Ray> ray = m_camera.rayThrough(QPointF(x, y));
    if (!ray) {
        return std::nullopt;
    }
    if (m_shown) {
        const std::optional<render::SurfaceHit> hit =
            render::pickSurface(*m_shown, *ray, ignoreBody.toStdString());
        if (hit && hit->bodyIndex < 0) {
            return Landing{hit->point.x(), hit->point.y(), 0.0, {}};
        }
        // A flat top (normal straight up) takes the shape; a wall or a slope does not.
        if (hit && hit->normal.z() > 0.9995F) {
            return Landing{hit->point.x(), hit->point.y(), hit->point.z(),
                           QString::fromStdString(
                               m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].id)};
        }
        if (hit && onFaces && hit->bodyIndex >= 0) {
            // Any other face: the shape lies on it. Rounding noise of the single-precision normal
            // goes (a wall is exactly a wall), or the angles would read 89.99999.
            const auto clean = [](float v) {
                return std::round(static_cast<double>(v) * 1e4) / 1e4;
            };
            double nx = clean(hit->normal.x());
            double ny = clean(hit->normal.y());
            double nz = clean(hit->normal.z());
            const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (length > 0.0) {
                Landing landing{hit->point.x(),
                                hit->point.y(),
                                hit->point.z(),
                                QString::fromStdString(
                                    m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].id),
                                nx / length,
                                ny / length,
                                nz / length,
                                true};
                return landing;
            }
        }
    }
    const std::optional<QVector3D> onPlane =
        m_camera.pointOnPlane(QPointF(x, y), static_cast<float>(planeZ));
    if (!onPlane) {
        return std::nullopt;
    }
    return Landing{onPlane->x(), onPlane->y(), planeZ, {}};
}

bool Viewport3D::lookAtSurface(double x, double y) {
    const std::optional<render::SurfaceHit> hit = surfaceAt(x, y);
    if (!hit) {
        return false;
    }
    QVector3D center = hit->point;
    if (hit->bodyIndex >= 0) {
        if (const auto shape = render::faceShape(
                m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].geometry(),
                hit->faceId)) {
            center = shape->center;
        }
    }
    render::CameraPose pose = m_camera.poseLookingFrom(hit->normal);
    pose.target = center;
    animateTo(pose);
    return true;
}

bool Viewport3D::lookAtFace(const QString& body, int face) {
    if (!m_shown || face <= 0) {
        return false;
    }
    const std::string id = body.toStdString();
    for (const render::RenderBody& drawn : m_shown->bodies) {
        if (drawn.id != id) {
            continue;
        }
        const auto shape = render::faceShape(drawn.geometry(), static_cast<std::uint32_t>(face));
        if (!shape) {
            return false;
        }
        render::CameraPose pose = m_camera.poseLookingFrom(shape->normal);
        pose.target = shape->center;
        animateTo(pose);
        return true;
    }
    return false;
}

bool Viewport3D::zoomToSurface(double x, double y) {
    const std::optional<render::SurfaceHit> hit = surfaceAt(x, y);
    if (!hit) {
        return false;
    }
    if (hit->bodyIndex >= 0) {
        if (const auto shape = render::faceShape(
                m_shown->bodies[static_cast<std::size_t>(hit->bodyIndex)].geometry(),
                hit->faceId)) {
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

Viewport3D::Snapshot Viewport3D::takeSnapshot(float aspect, bool pickIdle) {
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
    snapshot.snapGridStep = m_snapGridShown ? static_cast<float>(m_snapGridStep) : 0.0F;
    snapshot.ambientOcclusion = m_ambientOcclusion;
    snapshot.sceneVersion = m_sceneVersion;
    snapshot.paletteVersion = m_paletteVersion;
    snapshot.highlights = m_selectionHighlights;
    if (m_hover) {
        // What is chosen already is drawn as chosen, not twice.
        const bool chosen = std::any_of(m_selectionHighlights.begin(), m_selectionHighlights.end(),
                                        [this](const render::Highlight& h) {
                                            return h.body == m_hover->body &&
                                                   h.kind == m_hover->kind &&
                                                   h.number == m_hover->number;
                                        });
        if (!chosen) {
            snapshot.highlights.push_back(*m_hover);
        }
    }
    snapshot.handles = m_handleGraphics[0];
    for (std::size_t source = 1; source < m_handleGraphics.size(); ++source) {
        snapshot.handles.append(m_handleGraphics[source]);
    }
    snapshot.handles.pixelScale =
        window() != nullptr ? static_cast<float>(window()->effectiveDevicePixelRatio()) : 1.0F;
    snapshot.accent = m_accent;
    snapshot.itemSize = QSizeF(width(), height());
    if (pickIdle && !m_pickQueue.empty()) {
        snapshot.pick = m_pickQueue.front();
        m_pickQueue.pop_front();
        ++m_picksInFlight;
    }
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
