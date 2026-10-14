// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/viewport_3d.h"

#include <QMouseEvent>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "fission/render/scene_renderer.h"

#include <QtQml/qqml.h>
#include <rhi/qrhi.h>

namespace fission::ui {

namespace {

constexpr int kAnimationMilliseconds = 180;
constexpr float kWheelStep = 0.9F;     // one notch of a mouse wheel zooms by 10 %
constexpr float kTrackpadOrbit = 0.6F; // two-finger drag: pixels to orbit pixels

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
        m_renderer->draw(cb, renderTarget(), frame);
        if (m_item != nullptr) {
            m_item->reportRendered(QString::fromUtf8(m_rhi->backendName()));
        }
    }

private:
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
            count += static_cast<int>(body.mesh.triangleCount());
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

void Viewport3D::cameraMoved() {
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
    const bool alt = event->modifiers().testFlag(Qt::AltModifier);
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (event->buttons().testFlag(Qt::RightButton) ||
        (event->buttons().testFlag(Qt::LeftButton) && alt)) {
        orbitBy(delta.x(), delta.y());
    } else if (event->buttons().testFlag(Qt::MiddleButton) ||
               (event->buttons().testFlag(Qt::LeftButton) && shift)) {
        panBy(delta.x(), delta.y());
    }
    event->accept();
}

void Viewport3D::wheelEvent(QWheelEvent* event) {
    const QPoint pixels = event->pixelDelta();
    if (!pixels.isNull()) {
        // Trackpad: two fingers turn the view, with Shift they move it.
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            panBy(pixels.x(), pixels.y());
        } else {
            orbitBy(pixels.x() * kTrackpadOrbit, pixels.y() * kTrackpadOrbit);
        }
    } else if (event->angleDelta().y() != 0) {
        // Mouse wheel: zoom towards the pointer; 120 units are one notch.
        const double notches = event->angleDelta().y() / 120.0;
        const double factor = std::pow(static_cast<double>(kWheelStep), notches);
        zoomBy(factor, event->position().x(), event->position().y());
    }
    event->accept();
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
