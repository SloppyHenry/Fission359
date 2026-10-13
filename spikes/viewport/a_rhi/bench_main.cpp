// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype A: headless benchmark. Same drawing and picking code as the Qt Quick
// item (MeshPass), rendered into a texture: no window, no display needed, so it also runs
// over SSH and while the screen is locked.
//
//   viewport_a_bench --backend metal|vulkan|opengl --scene part|bench0.5|bench2|bench5
//                    [--frames 300] [--size 1920x1080] [--samples 4] [--picks 200]
//                    [--screenshot file.png] [--result-file file.jsonl]

#include "mesh_pass.h"

#include "../common/orbit_camera.h"

#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

// Vulkan is used on Linux only (the Mac build of Qt has Metal).
#if defined(Q_OS_LINUX)
#include <QVulkanInstance>
#define FISSION_SPIKE_VULKAN 1
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using fission::spike::MeshPass;
using fission::spike::OrbitCamera;
using fission::spike::Scene;
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

struct Device {
    std::unique_ptr<QRhi> rhi;
    std::unique_ptr<QOffscreenSurface> surface;
#if defined(FISSION_SPIKE_VULKAN)
    std::unique_ptr<QVulkanInstance> vulkan;
#endif
};

Device createDevice(const QString& backend) {
    Device device;
    const QRhi::Flags flags = QRhi::EnableTimestamps;
    if (backend == "metal") {
#if defined(Q_OS_DARWIN)
        QRhiMetalInitParams params;
        device.rhi.reset(QRhi::create(QRhi::Metal, &params, flags));
#endif
    } else if (backend == "vulkan") {
#if defined(FISSION_SPIKE_VULKAN)
        device.vulkan = std::make_unique<QVulkanInstance>();
        if (!device.vulkan->create()) {
            std::fprintf(stderr, "Vulkan instance could not be created\n");
            return device;
        }
        QRhiVulkanInitParams params;
        params.inst = device.vulkan.get();
        device.rhi.reset(QRhi::create(QRhi::Vulkan, &params, flags));
#endif
    } else if (backend == "opengl") {
        device.surface.reset(QRhiGles2InitParams::newFallbackSurface());
        QRhiGles2InitParams params;
        params.fallbackSurface = device.surface.get();
        device.rhi.reset(QRhi::create(QRhi::OpenGLES2, &params, flags));
    }
    return device;
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {"backend", "metal | vulkan | opengl", "api", "metal"},
        {"scene", "part | bench0.5 | bench2 | bench5", "name", "part"},
        {"frames", "Frames to render", "n", "300"},
        {"size", "Image size WxH", "size", "1920x1080"},
        {"samples", "MSAA samples", "n", "4"},
        {"picks", "Pick requests to time", "n", "200"},
        {"screenshot", "Write one frame as PNG", "file", ""},
        {"result-file", "Append the result (JSON line) here", "file", ""},
    });
    parser.process(app);

    const QString backend = parser.value("backend").toLower();
    const int frames = parser.value("frames").toInt();
    const int samples = parser.value("samples").toInt();
    const int picks = parser.value("picks").toInt();
    const QStringList sizeParts = parser.value("size").split('x');
    const QSize size(sizeParts.value(0).toInt(), sizeParts.value(1).toInt());

    Device device = createDevice(backend);
    if (!device.rhi) {
        std::fprintf(stderr, "Backend '%s' is not available\n", qPrintable(backend));
        return 2;
    }
    QRhi* rhi = device.rhi.get();

    const auto sceneStart = Clock::now();
    Scene scene = fission::spike::makeScene(parser.value("scene").toStdString());
    if (scene.faceCount == 0) {
        std::fprintf(stderr, "Unknown scene\n");
        return 2;
    }
    const double sceneSeconds = millisecondsSince(sceneStart) / 1000.0;

    // render target: (MSAA color + resolve texture) + depth
    std::unique_ptr<QRhiTexture> color(rhi->newTexture(
        QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    color->create();
    std::unique_ptr<QRhiRenderBuffer> msaa;
    std::unique_ptr<QRhiRenderBuffer> depth(
        rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, samples));
    depth->create();
    QRhiColorAttachment attachment;
    if (samples > 1) {
        msaa.reset(rhi->newRenderBuffer(QRhiRenderBuffer::Color, size, samples));
        msaa->create();
        attachment.setRenderBuffer(msaa.get());
        attachment.setResolveTexture(color.get());
    } else {
        attachment.setTexture(color.get());
    }
    QRhiTextureRenderTargetDescription description(attachment);
    description.setDepthStencilBuffer(depth.get());
    std::unique_ptr<QRhiTextureRenderTarget> target(rhi->newTextureRenderTarget(description));
    std::unique_ptr<QRhiRenderPassDescriptor> pass(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    target->create();

    MeshPass mesh(rhi);
    mesh.createPipelines(pass.get(), samples);

    QRhiCommandBuffer* cb = nullptr;
    rhi->beginOffscreenFrame(&cb);
    mesh.upload(cb, scene);
    rhi->endOffscreenFrame();

    OrbitCamera camera;
    const auto& lo = scene.mesh.boundsMin;
    const auto& hi = scene.mesh.boundsMax;
    camera.frame(QVector3D(lo[0], lo[1], lo[2]), QVector3D(hi[0], hi[1], hi[2]));
    const float aspect = float(size.width()) / float(size.height());

    auto frameData = [&](int hover) {
        MeshPass::Frame frame;
        frame.view = camera.view();
        frame.projection = camera.projection(aspect);
        frame.hoverFace = hover;
        return frame;
    };
    const QColor clear = QColor::fromRgbF(0.075F, 0.082F, 0.098F, 1.0F);
    const bool timestamps = rhi->isFeatureSupported(QRhi::Timestamps);

    // --- frame time (drawing only), 10 warm-up frames not counted ---
    std::vector<double> frameMs;
    std::vector<double> gpuMs;
    for (int i = 0; i < frames + 10; ++i) {
        camera.orbit(1.5F, 0.0F);
        const auto start = Clock::now();
        rhi->beginOffscreenFrame(&cb);
        mesh.draw(cb, target.get(), frameData(0), clear);
        rhi->endOffscreenFrame();
        const double wall = millisecondsSince(start);
        if (i >= 10) {
            frameMs.push_back(wall);
            if (timestamps) {
                gpuMs.push_back(cb->lastCompletedGpuTime() * 1000.0);
            }
        }
    }

    // --- frame time including a readback of the finished image (what prototype B must do
    //     in headless mode, so this is the fair comparison) ---
    std::vector<double> readbackMs;
    for (int i = 0; i < 60; ++i) {
        camera.orbit(1.5F, 0.0F);
        QRhiReadbackResult image;
        const auto start = Clock::now();
        rhi->beginOffscreenFrame(&cb);
        mesh.draw(cb, target.get(), frameData(0), clear);
        QRhiResourceUpdateBatch* readback = rhi->nextResourceUpdateBatch();
        readback->readBackTexture(QRhiReadbackDescription(color.get()), &image);
        cb->resourceUpdate(readback);
        rhi->endOffscreenFrame();
        readbackMs.push_back(millisecondsSince(start));
    }

    // --- picking: ID-buffer pass + readback, as it happens on every mouse move ---
    std::vector<double> pickMs;
    int hits = 0;
    for (int i = 0; i < picks; ++i) {
        const QPoint pixel((i * 97 + 31) % size.width(), (i * 53 + 17) % size.height());
        const auto start = Clock::now();
        int face = -1;
        rhi->beginOffscreenFrame(&cb);
        mesh.pick(cb, size, pixel, frameData(0), [&face](int id) { face = id; });
        rhi->endOffscreenFrame();
        const double latency = millisecondsSince(start);
        if (face >= 0) {
            pickMs.push_back(latency);
            hits += face > 0 ? 1 : 0;
        }
    }

    // --- optional picture ---
    const QString shot = parser.value("screenshot");
    if (!shot.isEmpty()) {
        camera.setAngles(-50.0F, 30.0F);
        QRhiReadbackResult result;
        rhi->beginOffscreenFrame(&cb);
        mesh.draw(cb, target.get(), frameData(0), clear);
        QRhiResourceUpdateBatch* readback = rhi->nextResourceUpdateBatch();
        readback->readBackTexture(QRhiReadbackDescription(color.get()), &result);
        cb->resourceUpdate(readback);
        rhi->endOffscreenFrame();
        if (!result.data.isEmpty()) {
            QImage image(reinterpret_cast<const uchar*>(result.data.constData()),
                         result.pixelSize.width(), result.pixelSize.height(),
                         QImage::Format_RGBA8888);
            image = image.copy();
            if (rhi->isYUpInFramebuffer()) {
                image = image.flipped(Qt::Vertical);
            }
            image.save(shot);
        }
    }

    const double averageMs = mean(frameMs);
    char json[900];
    std::snprintf(
        json, sizeof json,
        "{\"prototype\":\"A-rhi\",\"mode\":\"headless\",\"backend\":\"%s\",\"device\":\"%s\","
        "\"scene\":\"%s\",\"triangles\":%zu,\"faces\":%d,\"size\":\"%dx%d\",\"samples\":%d,"
        "\"scene_build_s\":%.2f,\"frames\":%zu,\"fps_avg\":%.1f,\"frame_ms_avg\":%.2f,"
        "\"frame_ms_p95\":%.2f,\"frame_ms_readback_avg\":%.2f,\"gpu_ms_avg\":%.2f,\"pick_ms_avg\":%.2f,\"pick_ms_p95\":%.2f,"
        "\"picks\":%zu,\"pick_hits\":%d}",
        rhi->backendName(), qPrintable(QString::fromUtf8(rhi->driverInfo().deviceName)),
        scene.name.c_str(), scene.mesh.triangleCount(), scene.faceCount, size.width(),
        size.height(), samples, sceneSeconds, frameMs.size(),
        averageMs > 0 ? 1000.0 / averageMs : 0.0, averageMs, percentile(frameMs, 0.95),
        mean(readbackMs), gpuMs.empty() ? -1.0 : mean(gpuMs), mean(pickMs), percentile(pickMs, 0.95), pickMs.size(),
        hits);
    std::printf("BENCH %s\n", json);
    const QString resultFile = parser.value("result-file");
    if (!resultFile.isEmpty()) {
        QFile file(resultFile);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.write(QByteArray(json) + "\n");
        }
    }
    return 0;
}
