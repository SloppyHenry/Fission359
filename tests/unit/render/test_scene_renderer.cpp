// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The renderer keeps the buffers of every body and sends only what changed (AP-1.3b). The tests
// run on Qt's "null" graphics backend: nothing is drawn, but every upload is counted.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "fission/render/scene_renderer.h"

using namespace fission::render;

namespace {

std::shared_ptr<const fission::geo::TriangleMesh> triangle(float size) {
    auto mesh = std::make_shared<fission::geo::TriangleMesh>();
    mesh->positions = {{0, 0, 0}, {size, 0, 0}, {0, size, 0}};
    mesh->normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    mesh->indices = {0, 1, 2};
    mesh->faceIds = {1};
    return mesh;
}

RenderBody body(const std::string& id, std::uint64_t version, float size = 10.0F) {
    RenderBody result;
    result.id = id;
    result.meshVersion = version;
    result.mesh = triangle(size);
    return result;
}

struct Fixture {
    std::unique_ptr<QRhi> rhi;
    std::unique_ptr<SceneRenderer> renderer;

    Fixture() {
        QRhiNullInitParams params;
        rhi.reset(QRhi::create(QRhi::Null, &params));
        REQUIRE(rhi != nullptr);
        renderer = std::make_unique<SceneRenderer>(rhi.get());
    }

    SceneRenderer::UploadStats upload(const RenderScene& scene, const ViewPalette& palette = {}) {
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
        renderer->upload(cb, scene, palette);
        rhi->endOffscreenFrame();
        return renderer->lastUploadStats();
    }
};

} // namespace

TEST_CASE("only a changed body is sent again", "[render][scene-renderer]") {
    Fixture f;
    RenderScene scene;
    scene.bodies = {body("a", 1), body("b", 1), body("c", 1)};

    auto stats = f.upload(scene);
    CHECK(stats.uploaded == 3);
    CHECK(stats.kept == 0);
    CHECK(f.renderer->bodyTriangleCount() == 3);

    // the same scene again: nothing to send
    stats = f.upload(scene);
    CHECK(stats.uploaded == 0);
    CHECK(stats.kept == 3);

    // body "b" got a new tessellation
    scene.bodies[1] = body("b", 2, 20.0F);
    stats = f.upload(scene);
    CHECK(stats.uploaded == 1);
    CHECK(stats.kept == 2);
}

TEST_CASE("a new colour or a removed body is handled body by body", "[render][scene-renderer]") {
    Fixture f;
    RenderScene scene;
    scene.bodies = {body("a", 1), body("b", 1)};
    f.upload(scene);

    scene.bodies[0].color = QColor(200, 30, 30);
    auto stats = f.upload(scene);
    CHECK(stats.uploaded == 1);
    CHECK(stats.kept == 1);

    scene.bodies.pop_back();
    stats = f.upload(scene);
    CHECK(stats.uploaded == 0);
    CHECK(stats.kept == 1);
    CHECK(stats.removed == 1);
    CHECK(f.renderer->bodyTriangleCount() == 1);

    scene.bodies.clear();
    stats = f.upload(scene);
    CHECK(stats.removed == 1);
    CHECK(f.renderer->bodyTriangleCount() == 0);
}

TEST_CASE("a new edge colour rebuilds the line buffers of all bodies", "[render][scene-renderer]") {
    Fixture f;
    RenderScene scene;
    scene.bodies = {body("a", 1), body("b", 1)};
    ViewPalette palette;
    f.upload(scene, palette);

    palette.edgeLine = QColor(255, 255, 255, 200);
    const auto stats = f.upload(scene, palette);
    CHECK(stats.uploaded == 2);
}

TEST_CASE("bodies without an id are told apart by their place", "[render][scene-renderer]") {
    Fixture f;
    RenderScene scene;
    scene.bodies = {body("", 1), body("", 1)};
    const auto stats = f.upload(scene);
    CHECK(stats.uploaded == 2);
    CHECK(f.renderer->bodyTriangleCount() == 2);
}

TEST_CASE("a frame with soft shadows runs through all passes and follows a resize",
          "[render][scene-renderer]") {
    Fixture f;
    RenderScene scene;
    scene.bodies = {body("a", 1)};

    for (const QSize size : {QSize(320, 200), QSize(640, 400)}) {
        std::unique_ptr<QRhiTexture> color(
            f.rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
        REQUIRE(color->create());
        std::unique_ptr<QRhiRenderBuffer> depth(
            f.rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
        REQUIRE(depth->create());
        QRhiTextureRenderTargetDescription description{QRhiColorAttachment(color.get())};
        description.setDepthStencilBuffer(depth.get());
        std::unique_ptr<QRhiTextureRenderTarget> target(f.rhi->newTextureRenderTarget(description));
        std::unique_ptr<QRhiRenderPassDescriptor> pass(target->newCompatibleRenderPassDescriptor());
        target->setRenderPassDescriptor(pass.get());
        REQUIRE(target->create());

        if (!f.renderer->hasPipelines()) {
            f.renderer->createPipelines(pass.get(), 1);
        }
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(f.rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
        f.renderer->upload(cb, scene, {});
        SceneRenderer::Frame frame;
        frame.projection.perspective(45.0F, float(size.width()) / float(size.height()), 1.0F,
                                     1000.0F);
        frame.view.lookAt({60, -80, 60}, {0, 0, 5}, {0, 0, 1});
        frame.ambientOcclusion = true;
        f.renderer->draw(cb, target.get(), frame);
        f.rhi->endOffscreenFrame();
    }
    SUCCEED();
}

TEST_CASE("handles are drawn twice: faint where a body hides them, full where they are seen",
          "[render][scene-renderer]") {
    Fixture f;
    RenderScene scene;
    scene.bodies = {body("a", 1)};
    const QSize size(320, 200);
    std::unique_ptr<QRhiTexture> color(
        f.rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
    REQUIRE(color->create());
    std::unique_ptr<QRhiRenderBuffer> depth(
        f.rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    REQUIRE(depth->create());
    QRhiTextureRenderTargetDescription description{QRhiColorAttachment(color.get())};
    description.setDepthStencilBuffer(depth.get());
    std::unique_ptr<QRhiTextureRenderTarget> target(f.rhi->newTextureRenderTarget(description));
    std::unique_ptr<QRhiRenderPassDescriptor> pass(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    REQUIRE(target->create());
    f.renderer->createPipelines(pass.get(), 1);

    SceneRenderer::Frame frame;
    frame.projection.perspective(45.0F, 1.6F, 1.0F, 1000.0F);
    frame.view.lookAt({60, -80, 60}, {0, 0, 5}, {0, 0, 1});
    const auto drawFrame = [&] {
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(f.rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
        f.renderer->upload(cb, scene, {});
        f.renderer->draw(cb, target.get(), frame);
        f.rhi->endOffscreenFrame();
    };
    drawFrame();
    CHECK(f.renderer->handleVertexCount() == 0); // no handles, nothing drawn for them

    frame.handles.lines.push_back({{0, 0, 0}, {10, 0, 0}, 1.5F, QColor(229, 83, 75)});
    frame.handles.dots.push_back({{5, 5, 0}, 7.0F, Qt::white, QColor(229, 83, 75), 2.0F, true});
    frame.handles.arrows.push_back({{5, 5, 0}, {1, 0, 0}, QColor(229, 83, 75), Qt::white});
    drawFrame();
    // One strip is two triangles; each half holds every shape once.
    CHECK(f.renderer->handleVertexCount() > 2 * 6);
    CHECK(f.renderer->handleVertexCount() % 2 == 0);
}
