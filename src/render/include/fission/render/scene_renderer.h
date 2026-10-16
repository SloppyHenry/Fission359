// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Draws the print bed and the bodies with Qt RHI (Metal, Vulkan, OpenGL, D3D). The class knows
// nothing about windows: the Qt Quick item in the ui layer and the tests use it the same way.

#pragma once

#include <QMatrix4x4>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fission/core/element_names.h"
#include "fission/core/selection.h"
#include "fission/render/pick.h"
#include "fission/render/scene.h"

#include <rhi/qrhi.h>
#include <rhi/qshader.h>

namespace fission::render {

/// How bodies are drawn: only their edges, in one neutral colour, or in their own colours.
enum class DisplayMode { wireframe, solid, colors };

/// One element drawn highlighted: under the pointer, or chosen. A body is highlighted as a whole.
struct Highlight {
    std::string body; ///< `RenderBody::id`
    core::SelectableKind kind = core::SelectableKind::body;
    std::uint32_t number = 0; ///< face, edge or corner number; 0 for a body
    bool hover = false;       ///< under the pointer (lighter) instead of chosen
};

class SceneRenderer {
public:
    struct Frame {
        QMatrix4x4 view;
        QMatrix4x4 projection;
        float pixelsPerMillimetre = 2.0F; ///< decides how fine the bed grid is
        bool bedVisible = true;
        DisplayMode displayMode = DisplayMode::colors;
        ViewPalette palette;
        /// Soft shadows in corners and crevices (ambient occlusion). Costs one more pass over
        /// the scene; the caller turns it off when the graphics card cannot keep up.
        bool ambientOcclusion = true;
        /// Highlights drawn over the bodies, in the colour of the accent.
        std::vector<Highlight> highlights;
        QColor accent{0x4C, 0x8D, 0xFF};
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

    /// The picture of a pick: `kPickSize`² numbers, top row first (see `choosePicked`).
    using PickCallback = std::function<void(std::vector<std::uint32_t> picture)>;
    /// Draws the faces (and, as far as the filter allows, the edges and corners) of all bodies
    /// as numbers into a small picture around `pixel` of a target of `targetSize`. The picture
    /// is handed to `done` once the graphics card has finished, which is at the end of this
    /// frame or a later one. Returns false (and does nothing) when the last pick is still on
    /// its way or there is nothing to pick.
    bool pick(QRhiCommandBuffer* cb, const QSize& targetSize, const QPoint& pixel,
              const QMatrix4x4& view, const QMatrix4x4& projection,
              const core::SelectionFilter& filter, PickCallback done);
    /// Like `pick`, but for a rectangle of pixels (a rubber band): the picture has the size of the
    /// rectangle, clipped to the target. Returns false if the rectangle is empty or too large.
    bool pickRegion(QRhiCommandBuffer* cb, const QSize& targetSize, const QRect& area,
                    const QMatrix4x4& view, const QMatrix4x4& projection,
                    const core::SelectionFilter& filter, PickCallback done);
    bool pickInFlight() const { return m_pickInFlight; }
    /// The id of the body drawn with this slot number in the pick picture, or empty.
    std::string bodyOfSlot(unsigned slot) const;
    /// The slot number of a body, or 0 if it has none (too many bodies).
    unsigned slotOfBody(const std::string& id) const;

private:
    void writeUniforms(QRhiResourceUpdateBatch* updates, QRhiBuffer* buffer, const QMatrix4x4& mvp,
                       const Frame& frame, const QSize& targetSize = {},
                       bool occlusion = false) const;
    /// Depth of the scene, then the occlusion from it, then the blur; fills `m_aoBlurTexture`.
    void renderAmbientOcclusion(QRhiCommandBuffer* cb, const Frame& frame, const QSize& size);
    bool ensureAmbientOcclusionTargets(const QSize& size);
    std::unique_ptr<QRhiBuffer> makeVertexBuffer(QRhiResourceUpdateBatch* updates, const void* data,
                                                 std::size_t bytes, QRhiBuffer::UsageFlags usage);
    std::unique_ptr<QRhiGraphicsPipeline>
    makePipeline(const QShader& vertex, const QShader& fragment, bool colorLayoutOnly, bool lines,
                 bool blend, bool depth, QRhiRenderPassDescriptor* pass, int samples,
                 QRhiShaderResourceBindings* srb = nullptr);

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
        // --- picking and highlights ---
        unsigned slot = 0; ///< number in the pick picture; 0: this body cannot be picked
        std::shared_ptr<const geo::TriangleMesh> mesh;
        std::unique_ptr<QRhiBuffer> faceNumbers; ///< one pick number per vertex
        std::unique_ptr<QRhiBuffer> edgePick;    ///< the edge lines with their pick numbers
        std::unique_ptr<QRhiBuffer> cornerPick;  ///< a small square per corner
        std::size_t edgePickCount = 0;
        std::size_t cornerPickCount = 0;
        /// Where the triangles of each face are in the index buffer, sorted by face.
        struct FaceRun {
            std::uint32_t face = 0;
            std::uint32_t firstTriangle = 0;
            std::uint32_t triangleCount = 0;
        };
        std::vector<FaceRun> faceRuns;
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
    std::unique_ptr<QRhiSampler> m_linearSampler;
    // ambient occlusion: depth of the scene (full size), occlusion and its blur (half size)
    QSize m_aoSize;
    float m_aoRadius = 6.0F; ///< mm; follows the size of the scene
    bool m_whiteUploaded = false;
    std::unique_ptr<QRhiTexture> m_aoWhite;
    std::unique_ptr<QRhiTexture> m_aoDepthTexture;
    std::unique_ptr<QRhiTexture> m_aoNormalTexture;
    std::unique_ptr<QRhiRenderPassDescriptor> m_aoDepthPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_aoDepthPipeline;
    std::unique_ptr<QRhiTexture> m_aoRawTexture;
    std::unique_ptr<QRhiTexture> m_aoBlurTexture;
    std::unique_ptr<QRhiTextureRenderTarget> m_aoDepthTarget;
    std::unique_ptr<QRhiTextureRenderTarget> m_aoRawTarget;
    std::unique_ptr<QRhiTextureRenderTarget> m_aoBlurTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_aoColorPass;
    std::unique_ptr<QRhiBuffer> m_aoDepthUniforms;
    std::unique_ptr<QRhiBuffer> m_aoRawUniforms;
    std::unique_ptr<QRhiBuffer> m_aoBlurUniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_aoDepthSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_aoRawSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_aoBlurSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_aoRawPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_aoBlurPipeline;
    QShader m_fullscreenVertex, m_ssaoFragment, m_aoBlurFragment, m_normalFragment;
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
    std::unique_ptr<QRhiGraphicsPipeline> m_overlayPipeline;

    QShader m_bodyVertex, m_bodyFragment, m_bedFragment, m_flatVertex, m_flatFragment,
        m_backgroundVertex, m_backgroundFragment, m_shadowVertex, m_shadowFragment;

    // --- pick pass ---
    void createPickResources();
    bool pickPass(QRhiCommandBuffer* cb, const QSize& targetSize, const QRect& window,
                  const QMatrix4x4& view, const QMatrix4x4& projection,
                  const core::SelectionFilter& filter, PickCallback done,
                  QRhiTextureRenderTarget* target, QRhiTexture* texture);
    static constexpr int kRegionMaxSize = 4096;
    /// The pick numbers of a body's vertices, edges and corners, and where its faces are.
    void buildPickBuffers(QRhiResourceUpdateBatch* updates, const geo::TriangleMesh& mesh,
                          GpuBody& body);
    std::map<unsigned, std::string> m_slotKeys;
    std::unique_ptr<QRhiTexture> m_pickTexture;
    std::unique_ptr<QRhiRenderBuffer> m_pickDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_pickTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_pickPass;
    std::unique_ptr<QRhiBuffer> m_pickUniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_pickSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_pickFacePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_pickEdgePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_pickCornerPipeline;
    QShader m_pickVertex, m_pickCornerVertex, m_pickFragment, m_overlayVertex, m_overlayFragment;
    std::unique_ptr<QRhiTexture> m_regionTexture;
    std::unique_ptr<QRhiRenderBuffer> m_regionDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_regionTarget;
    QRhiReadbackResult m_pickResult;
    PickCallback m_pickDone;
    bool m_pickInFlight = false;

    // --- highlights ---
    /// Works out what to draw for `frame.highlights` and uploads it. Before the pass starts.
    void prepareHighlights(QRhiResourceUpdateBatch* updates, const Frame& frame,
                           const QSize& targetSize);
    /// Draws what `prepareHighlights` made. Inside the pass.
    void drawHighlights(QRhiCommandBuffer* cb);
    struct TintDraw {
        const GpuBody* body = nullptr;
        int style = 0;          ///< index into the overlay colours
        std::uint32_t face = 0; ///< the face to tint; 0: the whole body
    };
    std::vector<TintDraw> m_tintDraws;
    std::size_t m_ribbonVertexCount = 0;
    static constexpr int kOverlayStyles = 4;
    quint32 m_overlayStride = 0;
    std::unique_ptr<QRhiBuffer> m_overlayUniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_overlaySrb;
    std::unique_ptr<QRhiBuffer> m_ribbonBuffer;
    std::size_t m_ribbonCapacity = 0; ///< vertices
};

} // namespace fission::render
