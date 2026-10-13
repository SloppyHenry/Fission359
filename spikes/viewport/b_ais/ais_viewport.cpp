// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ais_viewport.h"

#include "../common/run_options.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Graphic3d_RenderingParams.hxx>
#include <NCollection_IndexedMap.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_View.hxx>
#include <OpenGl_Window.hxx>
#include <Prs3d_Drawer.hxx>
#include <Quantity_Color.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <QCoreApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPointer>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>

#if defined(Q_OS_LINUX)
#include <GL/glx.h> // OpenCASCADE needs the X display and drawable of Qt's context
#endif

namespace fission::spike {

namespace {

class AisRenderer : public QQuickFramebufferObject::Renderer {
public:
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

    void synchronize(QQuickFramebufferObject* item) override {
        m_item = static_cast<AisViewport*>(item);
        m_snapshot = m_item->takeSnapshot();
    }

    void render() override {
        if (!m_snapshot.scene) {
            return;
        }
        if (!m_ready) {
            initialize();
        }
        const QSize size = framebufferObject()->size();
        if (size != m_size) {
            m_size = size;
            m_window->SetSize(size.width(), size.height());
            m_view->MustBeResized();
        }

        auto* camera = m_view->Camera().get();
        camera->SetEyeAndCenter(
            gp_Pnt(m_snapshot.eye.x(), m_snapshot.eye.y(), m_snapshot.eye.z()),
            gp_Pnt(m_snapshot.target.x(), m_snapshot.target.y(), m_snapshot.target.z()));

        framebufferObject()->bind();
        m_defaultBuffer->InitWrapper(m_viewContext);
        m_view->Invalidate();
        m_view->Redraw();

        if (m_snapshot.pickRequested) {
            m_context->MoveTo(m_snapshot.pickPixel.x(), m_snapshot.pickPixel.y(), m_view, false);
            int face = 0;
            if (m_context->HasDetected()) {
                const auto owner = occ::down_cast<StdSelect_BRepOwner>(m_context->DetectedOwner());
                if (!owner.IsNull() && owner->HasShape()) {
                    face = m_faces.FindIndex(owner->Shape());
                }
            }
            bool selected = false;
            if (m_snapshot.clicked) {
                m_context->SelectDetected();
                selected = true;
            }
            const double latency =
                double(m_item->nowNs() - m_snapshot.requestedAtNs) / 1.0e6;
            m_item->reportPick(face, selected, latency);
        }
        m_item->reportFrame();
        // OpenCASCADE changed the OpenGL state: hand a clean state back to Qt Quick.
        QQuickOpenGLUtils::resetOpenGLState();
        update();
    }

private:
    void initialize() {
        m_ready = true;
        const Scene& scene = *m_snapshot.scene;
        TopExp::MapShapes(scene.shape, TopAbs_FACE, m_faces);

#if defined(Q_OS_LINUX)
        // OpenCASCADE's GLX code must use the display and drawable Qt's context belongs to.
        Display* xDisplay = glXGetCurrentDisplay();
        occ::handle<Aspect_DisplayConnection> display =
            xDisplay != nullptr ? new Aspect_DisplayConnection(reinterpret_cast<Aspect_XDisplay*>(xDisplay))
                                : new Aspect_DisplayConnection();
#else
        occ::handle<Aspect_DisplayConnection> display = new Aspect_DisplayConnection();
#endif
        occ::handle<OpenGl_GraphicDriver> driver = new OpenGl_GraphicDriver(display, false);
        driver->ChangeOptions().buffersNoSwap = true;
        occ::handle<V3d_Viewer> viewer = new V3d_Viewer(driver);
        viewer->SetDefaultLights();
        viewer->SetLightOn();
        m_context = new AIS_InteractiveContext(viewer);
        m_view = viewer->CreateView();
        m_view->SetImmediateUpdate(false);
        m_view->ChangeRenderingParams().NbMsaaSamples = runOptions().samples;
        m_view->SetBackgroundColor(Quantity_Color(0.075, 0.082, 0.098, Quantity_TOC_sRGB));

        occ::handle<OpenGl_Context> glContext = new OpenGl_Context();
        glContext->Init(true);
        m_window = new Aspect_NeutralWindow();
#if defined(Q_OS_LINUX)
        m_window->SetNativeHandle(static_cast<Aspect_Drawable>(glXGetCurrentDrawable()));
#endif
        m_window->SetSize(1, 1);
        m_view->SetWindow(m_window, glContext->RenderingContext());

        occ::handle<OpenGl_View> glView = occ::down_cast<OpenGl_View>(m_view->View());
        m_viewContext = glView->GlWindow()->GetGlContext();
        m_defaultBuffer = m_viewContext->DefaultFrameBuffer();
        if (m_defaultBuffer.IsNull()) {
            m_defaultBuffer = new OpenGl_FrameBuffer();
            m_viewContext->SetDefaultFrameBuffer(m_defaultBuffer);
        }

        const occ::handle<Graphic3d_Camera>& camera = m_view->Camera();
        camera->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
        camera->SetFOVy(45.0);
        camera->SetUp(gp_Dir(0, 0, 1));
        camera->SetZRange(1.0, 1.0e5);

        occ::handle<AIS_Shape> shape = new AIS_Shape(scene.shape);
        shape->Attributes()->SetFaceBoundaryDraw(true);
        // same base color as prototype A (the AIS default material is orange brass)
        shape->SetColor(Quantity_Color(0.62, 0.66, 0.72, Quantity_TOC_sRGB));
        shape->SetMaterial(Graphic3d_MaterialAspect(Graphic3d_NameOfMaterial_Plastified));
        m_context->Display(shape, AIS_Shaded, 0, false);
        m_context->Deactivate();
        m_context->Activate(shape, AIS_Shape::SelectionMode(TopAbs_FACE));
    }

    bool m_ready = false;
    AisViewport* m_item = nullptr;
    AisViewport::Snapshot m_snapshot;
    QSize m_size;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> m_faces;
    occ::handle<AIS_InteractiveContext> m_context;
    occ::handle<V3d_View> m_view;
    occ::handle<Aspect_NeutralWindow> m_window;
    occ::handle<OpenGl_Context> m_viewContext;
    occ::handle<OpenGl_FrameBuffer> m_defaultBuffer;
};

} // namespace

AisViewport::AisViewport(QQuickItem* parent) : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setMirrorVertically(true); // OpenGL framebuffers are bottom-up
    m_clock.start();
    auto scene = std::make_shared<Scene>(makeScene(runOptions().scene.toStdString()));
    if (scene->faceCount == 0) {
        scene = std::make_shared<Scene>(makeScene("part"));
    }
    qInfo("scene %s: %zu triangles, %d faces", scene->name.c_str(), scene->mesh.triangleCount(),
          scene->faceCount);
    m_scene = scene;
    const auto& lo = scene->mesh.boundsMin;
    const auto& hi = scene->mesh.boundsMax;
    m_camera.frame(QVector3D(lo[0], lo[1], lo[2]), QVector3D(hi[0], hi[1], hi[2]));
}

QQuickFramebufferObject::Renderer* AisViewport::createRenderer() const {
    return new AisRenderer;
}

void AisViewport::zoomBy(double factor) {
    m_camera.zoom(static_cast<float>(factor));
    update();
}

void AisViewport::mousePressEvent(QMouseEvent* event) {
    m_lastMouse = event->position();
    m_dragged = false;
    event->accept();
}

void AisViewport::mouseMoveEvent(QMouseEvent* event) {
    const QPointF delta = event->position() - m_lastMouse;
    m_lastMouse = event->position();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool alt = event->modifiers().testFlag(Qt::AltModifier);
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

void AisViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_dragged && event->button() == Qt::LeftButton) {
        m_clickWanted = true;
        m_pickWanted = true;
        m_pickPixel = event->position().toPoint();
        m_pickWantedAtNs = m_clock.nsecsElapsed();
    }
    update();
}

void AisViewport::wheelEvent(QWheelEvent* event) {
    const QPoint pixels = event->pixelDelta();
    if (!pixels.isNull()) {
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

void AisViewport::hoverMoveEvent(QHoverEvent* event) {
    m_pickWanted = true;
    m_pickPixel = event->position().toPoint();
    m_pickWantedAtNs = m_clock.nsecsElapsed();
    update();
}

AisViewport::Snapshot AisViewport::takeSnapshot() {
    Snapshot snapshot;
    snapshot.scene = m_scene;
    snapshot.eye = m_camera.eye();
    snapshot.target = m_camera.target();
    if (m_pickWanted) {
        const double dpr = window() ? window()->devicePixelRatio() : 1.0;
        snapshot.pickRequested = true;
        snapshot.pickPixel = QPoint(int(m_pickPixel.x() * dpr), int(m_pickPixel.y() * dpr));
        snapshot.clicked = m_clickWanted;
        snapshot.requestedAtNs = m_pickWantedAtNs;
        m_pickWanted = false;
        m_clickWanted = false;
    }
    return snapshot;
}

void AisViewport::reportFrame() {
    const qint64 now = m_clock.nsecsElapsed();
    if (!runOptions().screenshot.isEmpty() && ++m_screenshotFrames == 30) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                window()->grabWindow().save(runOptions().screenshot);
                QCoreApplication::quit();
            },
            Qt::QueuedConnection);
    }
    if (m_lastFrameNs != 0) {
        m_intervalsMs.push_back(double(now - m_lastFrameNs) / 1.0e6);
    }
    m_lastFrameNs = now;
    if (now - m_lastStatsNs > 500'000'000 && !m_intervalsMs.empty()) {
        double sum = 0.0;
        for (const double value : m_intervalsMs) {
            sum += value;
        }
        m_fps = 1000.0 / (sum / double(m_intervalsMs.size()));
        m_intervalsMs.clear();
        m_lastStatsNs = now;
        QMetaObject::invokeMethod(this, [this] { emit statsChanged(); }, Qt::QueuedConnection);
    }
}

void AisViewport::reportPick(int faceId, bool selected, double latencyMs) {
    QMetaObject::invokeMethod(
        this,
        [this, faceId, selected, latencyMs] {
            if (faceId != m_hoverFace) {
                m_hoverFace = faceId;
                emit hoverFaceChanged();
            }
            if (selected) {
                m_selectedFace = faceId;
                emit selectedFaceChanged();
            }
            m_pickLatencyMs = 0.9 * m_pickLatencyMs + 0.1 * latencyMs;
        },
        Qt::QueuedConnection);
}

} // namespace fission::spike
