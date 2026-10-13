// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype A: the drawing and picking code on top of Qt RHI. It does not know
// about windows, so the same code serves the Qt Quick item and the headless benchmark.

#pragma once

#include "../common/scene.h"

#include <QColor>
#include <QMatrix4x4>
#include <QPoint>
#include <QSize>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <functional>
#include <memory>

namespace fission::spike {

class MeshPass {
public:
    struct Frame {
        QMatrix4x4 view;
        QMatrix4x4 projection;
        int hoverFace = 0;
        int selectedFace = 0;
    };

    // Creates buffers, the pick target and loads the shaders (":/shaders/*.qsb").
    explicit MeshPass(QRhi* rhi);

    // The pipelines depend on the render pass of the target that is drawn into.
    void createPipelines(QRhiRenderPassDescriptor* pass, int sampleCount);
    bool hasPipelines() const { return m_pipeline != nullptr; }

    // Uploads vertex data (once per scene). Needs an open command buffer.
    void upload(QRhiCommandBuffer* cb, const Scene& scene);
    const Scene* uploadedScene() const { return m_uploaded; }

    // Renders the shadow map (call after upload, and again when the scene changes).
    void renderShadow(QRhiCommandBuffer* cb);

    // Draws the scene into `target`.
    void draw(QRhiCommandBuffer* cb, QRhiRenderTarget* target, const Frame& frame,
              const QColor& clearColor);

    // Renders just the pixel `pixel` of a `targetSize` image into the pick target and reads
    // the face id back. `done` is called with the face id (0 = background) when the data is
    // there. Returns false if a pick is still in flight.
    bool pick(QRhiCommandBuffer* cb, const QSize& targetSize, const QPoint& pixel,
              const Frame& frame, std::function<void(int)> done);
    bool pickInFlight() const { return m_pickInFlight; }

private:
    void writeUniforms(QRhiResourceUpdateBatch* updates, QRhiBuffer* buffer, const QMatrix4x4& mvp,
                       const Frame& frame);
    std::unique_ptr<QRhiGraphicsPipeline> makePipeline(const QShader& vertex,
                                                       const QShader& fragment, bool meshLayout,
                                                       bool lines, QRhiRenderPassDescriptor* pass,
                                                       int samples,
                                                       QRhiShaderResourceBindings* bindings);

    QRhi* m_rhi;
    const Scene* m_uploaded = nullptr;
    std::size_t m_indexCount = 0;
    std::size_t m_edgeVertexCount = 0;

    std::unique_ptr<QRhiBuffer> m_vertexBuffer;
    std::unique_ptr<QRhiBuffer> m_indexBuffer;
    std::unique_ptr<QRhiBuffer> m_edgeBuffer;
    std::unique_ptr<QRhiBuffer> m_bedVertexBuffer;
    std::unique_ptr<QRhiBuffer> m_bedIndexBuffer;
    std::unique_ptr<QRhiBuffer> m_shadowUniforms;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiTexture> m_shadowTexture;
    std::unique_ptr<QRhiTextureRenderTarget> m_shadowTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_shadowPass;
    std::unique_ptr<QRhiShaderResourceBindings> m_shadowSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_shadowPipeline;
    QMatrix4x4 m_lightViewProjection;
    QVector3D m_lightDirection;
    std::unique_ptr<QRhiBuffer> m_uniforms;
    std::unique_ptr<QRhiBuffer> m_pickUniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiShaderResourceBindings> m_pickSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_linePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_pickPipeline;

    std::unique_ptr<QRhiTexture> m_pickTexture;
    std::unique_ptr<QRhiRenderBuffer> m_pickDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_pickTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_pickPass;
    QRhiReadbackResult m_pickResult;
    bool m_pickInFlight = false;
    std::function<void(int)> m_pickDone;

    QShader m_vertexShader;
    QShader m_meshFragment;
    QShader m_pickFragment;
    QShader m_lineVertex;
    QShader m_lineFragment;
    QShader m_shadowVertex;
    QShader m_shadowFragment;
};

} // namespace fission::spike
