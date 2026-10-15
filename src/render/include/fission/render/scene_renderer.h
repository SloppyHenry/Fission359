// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Draws the print bed and the bodies with Qt RHI (Metal, Vulkan, OpenGL, D3D). The class knows
// nothing about windows: the Qt Quick item in the ui layer and the tests use it the same way.

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fission/render/scene.h"

#include <rhi/qrhi.h>
#include <rhi/qshader.h>

namespace fission::render {

/// How bodies are drawn: only their edges, in one neutral colour, or in their own colours.
enum class DisplayMode { wireframe, solid, colors };

class SceneRenderer {
public:
    struct Frame {
        QMatrix4x4 view;
        QMatrix4x4 projection;
        float pixelsPerMillimetre = 2.0F; ///< decides how fine the bed grid is
        bool bedVisible = true;
        DisplayMode displayMode = DisplayMode::colors;
        ViewPalette palette;
    };

    /// Creates buffers and loads the shaders. Needs a current RHI.
    explicit SceneRenderer(QRhi* rhi);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    /// The pipelines depend on the render pass and sample count of the target drawn into.
    void createPipelines(QRhiRenderPassDescriptor* pass, int sampleCount);
    bool hasPipelines() const { return m_bodyPipeline != nullptr; }
    bool hasGeometry() const { return m_bedIndexCount > 0 || m_bodyIndexCount > 0; }

    /// Uploads geometry; call when the scene or the palette changed. Needs an open command buffer.
    void upload(QRhiCommandBuffer* cb, const RenderScene& scene, const ViewPalette& palette);

    /// Draws everything into `target`, including the background gradient.
    void draw(QRhiCommandBuffer* cb, QRhiRenderTarget* target, const Frame& frame);

    /// Triangles drawn per frame (bodies only), for the log and the tests.
    std::size_t bodyTriangleCount() const { return m_bodyIndexCount / 3; }

    /// What the last `upload` did with the bodies: only changed ones are sent to the GPU.
    struct UploadStats {
        int uploaded = 0; ///< new or changed bodies
        int kept = 0;     ///< unchanged bodies whose buffers stayed as they were
        int removed = 0;  ///< bodies that are gone
    };
    UploadStats lastUploadStats() const { return m_lastStats; }

private:
    void writeUniforms(QRhiResourceUpdateBatch* updates, QRhiBuffer* buffer, const QMatrix4x4& mvp,
                       const Frame& frame) const;
    std::unique_ptr<QRhiBuffer> makeVertexBuffer(QRhiResourceUpdateBatch* updates, const void* data,
                                                 std::size_t bytes, QRhiBuffer::UsageFlags usage);
    std::unique_ptr<QRhiGraphicsPipeline>
    makePipeline(const QShader& vertex, const QShader& fragment, bool colorLayoutOnly, bool lines,
                 bool blend, bool depth, QRhiRenderPassDescriptor* pass, int samples);

    /// The graphics buffers of one body.
    struct GpuBody {
        std::uint64_t meshVersion = 0;
        QColor color;
        std::unique_ptr<QRhiBuffer> vertices;
        std::unique_ptr<QRhiBuffer> indices;
        std::unique_ptr<QRhiBuffer> lines;
        std::size_t indexCount = 0;
        std::size_t lineVertexCount = 0;
        QVector3D boundsMin;
        QVector3D boundsMax;
    };

    QRhi* m_rhi;
    std::map<std::string, GpuBody> m_gpuBodies;
    std::vector<const GpuBody*> m_drawOrder;
    QColor m_edgeLineColor;
    UploadStats m_lastStats;
    std::size_t m_bodyIndexCount = 0;
    std::size_t m_bedIndexCount = 0;
    std::size_t m_discVertexCount = 0;
    std::size_t m_lineVertexCount = 0; ///< axes and the rim of the bed
    bool m_hasShadow = false;

    std::unique_ptr<QRhiBuffer> m_uniforms;
    std::unique_ptr<QRhiBuffer> m_shadowUniforms;
    std::unique_ptr<QRhiBuffer> m_bedVertices;
    std::unique_ptr<QRhiBuffer> m_bedIndices;
    std::unique_ptr<QRhiBuffer> m_discVertices;
    std::unique_ptr<QRhiBuffer> m_lineVertices;

    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiTexture> m_shadowTexture;
    std::unique_ptr<QRhiTextureRenderTarget> m_shadowTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_shadowPass;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiShaderResourceBindings> m_shadowSrb;
    QMatrix4x4 m_lightViewProjection;
    QVector3D m_lightDirection;

    std::unique_ptr<QRhiGraphicsPipeline> m_shadowPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_backgroundPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_bedPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_bodyPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_discPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_linePipeline;

    QShader m_bodyVertex, m_bodyFragment, m_bedFragment, m_flatVertex, m_flatFragment,
        m_backgroundVertex, m_backgroundFragment, m_shadowVertex, m_shadowFragment;
};

} // namespace fission::render
