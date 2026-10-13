// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype B: OpenCASCADE's own viewer (AIS / V3d / OpenGl) rendering into an
// OpenGL framebuffer that Qt created. This is the same embedding that a Qt Quick item
// (QQuickFramebufferObject) would use, without a window, so it runs while the screen is locked.
//
//   viewport_b_bench --scene part|bench0.5|bench2|bench5 [--frames 150] [--size 1920x1080]
//                    [--samples 4] [--picks 200] [--screenshot file.png] [--result-file f.jsonl]

#include "../common/orbit_camera.h"
#include "../common/scene.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Graphic3d_Camera.hxx>
#include <Image_AlienPixMap.hxx>
#include <Graphic3d_RenderingParams.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_View.hxx>
#include <OpenGl_Window.hxx>
#include <Prs3d_Drawer.hxx>
#include <Quantity_Color.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using fission::spike::OrbitCamera;
using Clock = std::chrono::steady_clock;

namespace {

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, std::size_t(fraction * double(values.size())))];
}

double mean(const std::vector<double>& values) {
    double sum = 0.0;
    for (const double value : values) {
        sum += value;
    }
    return values.empty() ? 0.0 : sum / double(values.size());
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {"scene", "part | bench0.5 | bench2 | bench5", "name", "part"},
        {"frames", "Frames to render", "n", "150"},
        {"size", "Image size WxH", "size", "1920x1080"},
        {"samples", "MSAA samples", "n", "4"},
        {"qt-context", "Use Qt's GL context and framebuffer (experimental, see notes)"},
        {"picks", "Pick requests to time", "n", "200"},
        {"screenshot", "Write one frame as PNG", "file", ""},
        {"result-file", "Append the result (JSON line) here", "file", ""},
    });
    parser.process(app);

    const int frames = parser.value("frames").toInt();
    const int samples = parser.value("samples").toInt();
    const int picks = parser.value("picks").toInt();
    const QStringList sizeParts = parser.value("size").split('x');
    const QSize size(sizeParts.value(0).toInt(), sizeParts.value(1).toInt());

    // --- Qt OpenGL context without a window ---
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    QOpenGLContext qtContext;
    qtContext.setFormat(format);
    if (!qtContext.create() || !qtContext.makeCurrent(&surface)) {
        std::fprintf(stderr, "No OpenGL context available\n");
        return 2;
    }
    QOpenGLFunctions* gl = qtContext.functions();
    const QString renderer = QString::fromUtf8(
        reinterpret_cast<const char*>(gl->glGetString(GL_RENDERER)));

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject fbo(size, fboFormat);

    // --- scene ---
    const auto sceneStart = Clock::now();
    fission::spike::Scene scene = fission::spike::makeScene(parser.value("scene").toStdString());
    if (scene.faceCount == 0) {
        std::fprintf(stderr, "Unknown scene\n");
        return 2;
    }
    const double sceneSeconds = millisecondsSince(sceneStart) / 1000.0;

    // --- OpenCASCADE viewer on top of Qt's context ---
    occ::handle<Aspect_DisplayConnection> display = new Aspect_DisplayConnection();
    occ::handle<OpenGl_GraphicDriver> driver = new OpenGl_GraphicDriver(display, false);
    driver->ChangeOptions().buffersNoSwap = true;
    occ::handle<V3d_Viewer> viewer = new V3d_Viewer(driver);
    viewer->SetDefaultLights();
    viewer->SetLightOn();
    occ::handle<AIS_InteractiveContext> context = new AIS_InteractiveContext(viewer);
    occ::handle<V3d_View> view = viewer->CreateView();
    view->SetImmediateUpdate(false);
    view->ChangeRenderingParams().NbMsaaSamples = samples;
    view->SetBackgroundColor(Quantity_Color(0.075, 0.082, 0.098, Quantity_TOC_sRGB));

    const bool ownContext = !parser.isSet("qt-context");
    occ::handle<Aspect_NeutralWindow> window = new Aspect_NeutralWindow();
    window->SetSize(size.width(), size.height());
    if (ownContext) {
        view->SetWindow(window); // OpenCASCADE creates and manages its own context
    } else {
        occ::handle<OpenGl_Context> glContext = new OpenGl_Context();
        if (!glContext->Init(true)) {
            std::fprintf(stderr, "OpenCASCADE could not use the OpenGL context\n");
            return 2;
        }
        view->SetWindow(window, glContext->RenderingContext());
    }
    view->MustBeResized();

    occ::handle<OpenGl_View> glView = occ::down_cast<OpenGl_View>(view->View());
    occ::handle<OpenGl_Context> viewContext = glView->GlWindow()->GetGlContext();
    occ::handle<OpenGl_FrameBuffer> defaultBuffer;
    if (!ownContext) {
        // The default framebuffer of OpenCASCADE is the Qt framebuffer object.
        defaultBuffer = viewContext->DefaultFrameBuffer();
        if (defaultBuffer.IsNull()) {
            defaultBuffer = new OpenGl_FrameBuffer();
            viewContext->SetDefaultFrameBuffer(defaultBuffer);
        }
    }

    Image_AlienPixMap framePixmap;
    auto renderFrame = [&] {
        if (!ownContext) {
            fbo.bind();
            defaultBuffer->InitWrapper(viewContext);
        }
        view->Invalidate();
        if (ownContext) {
            // OpenCASCADE renders into its own offscreen buffer and reads the image back.
            view->ToPixMap(framePixmap, size.width(), size.height());
        } else {
            view->Redraw();
        }
        viewContext->core11fwd->glFinish();
    };
    OrbitCamera camera;
    const auto& lo = scene.mesh.boundsMin;
    const auto& hi = scene.mesh.boundsMax;
    camera.frame(QVector3D(lo[0], lo[1], lo[2]), QVector3D(hi[0], hi[1], hi[2]));
    auto applyCamera = [&] {
        const QVector3D eye = camera.eye();
        const QVector3D center = camera.target();
        const occ::handle<Graphic3d_Camera>& cam = view->Camera();
        cam->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
        cam->SetFOVy(45.0);
        cam->SetUp(gp_Dir(0, 0, 1));
        cam->SetEyeAndCenter(gp_Pnt(eye.x(), eye.y(), eye.z()),
                             gp_Pnt(center.x(), center.y(), center.z()));
        cam->SetZRange(1.0, 1.0e5);
        view->Invalidate();
    };

    // --- display the shape (AIS builds the presentation, uploads, prepares selection) ---
    const auto displayStart = Clock::now();
    occ::handle<AIS_Shape> shape = new AIS_Shape(scene.shape);
    shape->Attributes()->SetFaceBoundaryDraw(true);
    context->Display(shape, AIS_Shaded, 0, false);
    context->Deactivate();
    context->Activate(shape, AIS_Shape::SelectionMode(TopAbs_FACE));
    applyCamera();
    renderFrame();
    const double displayMs = millisecondsSince(displayStart);

    // --- frame time ---
    std::vector<double> frameMs;
    for (int i = 0; i < frames + 10; ++i) {
        camera.orbit(1.5F, 0.0F);
        applyCamera();
        const auto start = Clock::now();
        renderFrame();
        const double wall = millisecondsSince(start);
        if (i >= 10) {
            frameMs.push_back(wall);
        }
    }

    // --- picking: MoveTo as on every mouse move ---
    std::vector<double> pickMs;
    int hits = 0;
    for (int i = 0; i < picks; ++i) {
        const int x = (i * 97 + 31) % size.width();
        const int y = (i * 53 + 17) % size.height();
        const auto start = Clock::now();
        context->MoveTo(x, y, view, false);
        const bool hit = context->HasDetected();
        pickMs.push_back(millisecondsSince(start));
        hits += hit ? 1 : 0;
    }

    const QString shot = parser.value("screenshot");
    if (!shot.isEmpty()) {
        camera.setAngles(-50.0F, 30.0F);
        applyCamera();
        context->ClearDetected();
        renderFrame();
        if (ownContext) {
            // OpenCASCADE can only write PPM without extra image libraries; Qt converts it.
            const QString ppm = shot + ".ppm";
            framePixmap.Save(TCollection_AsciiString(qPrintable(ppm)));
            QImage(ppm).save(shot);
            QFile::remove(ppm);
        } else {
            fbo.toImage().save(shot);
        }
    }

    const double averageMs = mean(frameMs);
    char json[900];
    std::snprintf(json, sizeof json,
                  "{\"prototype\":\"B-ais\",\"mode\":\"headless\",\"backend\":\"OpenGL%s\","
                  "\"device\":\"%s\",\"scene\":\"%s\",\"triangles\":%zu,\"faces\":%d,"
                  "\"size\":\"%dx%d\",\"samples\":%d,\"scene_build_s\":%.2f,\"display_ms\":%.0f,"
                  "\"frames\":%zu,\"fps_avg\":%.1f,\"frame_ms_avg\":%.2f,\"frame_ms_p95\":%.2f,"
                  "\"gpu_ms_avg\":-1,\"pick_ms_avg\":%.2f,\"pick_ms_p95\":%.2f,\"picks\":%zu,"
                  "\"pick_hits\":%d}",
                  ownContext ? "(own context)" : "(Qt context)", qPrintable(renderer),
                  scene.name.c_str(), scene.mesh.triangleCount(),
                  scene.faceCount, size.width(), size.height(), samples, sceneSeconds, displayMs,
                  frameMs.size(), averageMs > 0 ? 1000.0 / averageMs : 0.0, averageMs,
                  percentile(frameMs, 0.95), mean(pickMs), percentile(pickMs, 0.95),
                  pickMs.size(), hits);
    std::printf("BENCH %s\n", json);
    const QString resultFile = parser.value("result-file");
    if (!resultFile.isEmpty()) {
        QFile file(resultFile);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.write(QByteArray(json) + "\n");
        }
    }
    view->Remove();
    return 0;
}
