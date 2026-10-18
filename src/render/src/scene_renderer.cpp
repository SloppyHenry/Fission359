// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/scene_renderer.h"

#include <QFile>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <utility>
#include <vector>

namespace fission::render {

namespace {

constexpr int kShadowSize = 2048;
constexpr float kAxisAlpha = 0.8F;

// Mirrors the "Params" block of the shaders (std140: every member is a multiple of 16 bytes).
struct Params {
    float mvp[16];
    float lightVp[16];
    float corr[16];
    float eye[4];
    float lightDir[4];
    float bedColor[4];
    float gridColor[4];
    float gridMajor[4];
    float bgCenter[4];
    float bgEdge[4];
    float shadowColor[4];
    float params[4];
    float bedSize[4];
    float bodyOverride[4];
    float view[4];
};
static_assert(sizeof(Params) == 64 * 3 + 16 * 12, "Params must match the std140 block");

// The block of the ambient-occlusion shaders (fullscreen.vert, ssao.frag, aoblur.frag).
struct AoParams {
    float invViewProj[16];
    float corr[16];
    float eye[4];
    float a[4];
    float b[4];
};
static_assert(sizeof(AoParams) == 64 * 2 + 16 * 3, "AoParams must match the std140 block");

struct MeshVertex {
    float position[3];
    float normal[3];
    float color[4];
};
struct FlatVertex {
    float position[3];
    float color[4];
};
static_assert(sizeof(MeshVertex) == 40 && sizeof(FlatVertex) == 28);

// The pick pass: a position and the number of the element it belongs to.
struct PickVertex {
    float position[3];
    std::uint32_t id;
};
struct PickCornerVertex {
    float position[3];
    std::uint32_t id;
    float offset[2]; ///< from the corner point, in pixels
};
static_assert(sizeof(PickVertex) == 16 && sizeof(PickCornerVertex) == 24);

// Mirrors the block of pick.vert and pickcorner.vert.
struct PickParams {
    float mvp[16];
    float misc[4];
};
static_assert(sizeof(PickParams) == 64 + 16);

// Mirrors the block of overlay.vert and overlay.frag.
struct OverlayParams {
    float mvp[16];
    float color[4];
};
static_assert(sizeof(OverlayParams) == 64 + 16);

QShader loadShader(const char* name) {
    QFile file(QStringLiteral(":/fission/render/shaders/%1.qsb").arg(QLatin1String(name)));
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Shader missing: %s", name);
    }
    return QShader::fromSerialized(file.readAll());
}

void setColor(float (&target)[4], const QColor& color, float alphaScale = 1.0F) {
    target[0] = static_cast<float>(color.redF());
    target[1] = static_cast<float>(color.greenF());
    target[2] = static_cast<float>(color.blueF());
    target[3] = static_cast<float>(color.alphaF()) * alphaScale;
}

void copyMatrix(float (&target)[16], const QMatrix4x4& matrix) {
    std::memcpy(target, matrix.constData(), sizeof target);
}

FlatVertex flatVertex(float x, float y, float z, const QColor& color, float alphaScale = 1.0F) {
    FlatVertex vertex{};
    vertex.position[0] = x;
    vertex.position[1] = y;
    vertex.position[2] = z;
    setColor(vertex.color, color, alphaScale);
    return vertex;
}

MeshVertex meshVertex(float x, float y, float z, float nx, float ny, float nz) {
    MeshVertex vertex{};
    vertex.position[0] = x;
    vertex.position[1] = y;
    vertex.position[2] = z;
    vertex.normal[0] = nx;
    vertex.normal[1] = ny;
    vertex.normal[2] = nz;
    vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 1.0F;
    return vertex;
}

// Outline of the plate in the XY plane, counter-clockwise, corners rounded.
std::vector<QPointF> roundedRectangle(const BedSpec& bed) {
    constexpr int kSegments = 12;
    const float hx = 0.5F * bed.sizeX;
    const float hy = 0.5F * bed.sizeY;
    const float r = std::clamp(bed.cornerRadius, 0.0F, std::min(hx, hy));
    const struct {
        float cx, cy, start;
    } corners[4] = {{hx - r, -hy + r, -90.0F},
                    {hx - r, hy - r, 0.0F},
                    {-hx + r, hy - r, 90.0F},
                    {-hx + r, -hy + r, 180.0F}};
    std::vector<QPointF> outline;
    for (const auto& corner : corners) {
        if (r <= 0.0F) {
            outline.emplace_back(corner.cx, corner.cy);
            continue;
        }
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = (corner.start + 90.0F * static_cast<float>(i) / kSegments) *
                                std::numbers::pi_v<float> / 180.0F;
            outline.emplace_back(corner.cx + r * std::cos(angle), corner.cy + r * std::sin(angle));
        }
    }
    return outline;
}

} // namespace

SceneRenderer::SceneRenderer(QRhi* rhi) : m_rhi(rhi) {
    const auto stages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_uniforms.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Params)));
    m_uniforms->create();
    m_shadowUniforms.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Params)));
    m_shadowUniforms->create();

    m_shadowTexture.reset(m_rhi->newTexture(QRhiTexture::D32F, QSize(kShadowSize, kShadowSize), 1,
                                            QRhiTexture::RenderTarget));
    m_shadowTexture->create();
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_sampler->create();
    m_linearSampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                            QRhiSampler::None, QRhiSampler::ClampToEdge,
                                            QRhiSampler::ClampToEdge));
    m_linearSampler->create();
    // Until there is an occlusion image, the shaders get a white one (nothing is occluded).
    m_aoWhite.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
    m_aoWhite->create();
    QRhiTextureRenderTargetDescription shadowDescription;
    shadowDescription.setDepthTexture(m_shadowTexture.get());
    m_shadowTarget.reset(m_rhi->newTextureRenderTarget(shadowDescription));
    m_shadowPass.reset(m_shadowTarget->newCompatibleRenderPassDescriptor());
    m_shadowTarget->setRenderPassDescriptor(m_shadowPass.get());
    m_shadowTarget->create();

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_uniforms.get()),
         QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                   m_shadowTexture.get(), m_sampler.get()),
         QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                   m_aoWhite.get(), m_linearSampler.get())});
    m_srb->create();
    m_shadowSrb.reset(m_rhi->newShaderResourceBindings());
    m_shadowSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_shadowUniforms.get())});
    m_shadowSrb->create();

    m_bodyVertex = loadShader("body.vert");
    m_bodyFragment = loadShader("body.frag");
    m_bedFragment = loadShader("bed.frag");
    m_flatVertex = loadShader("flat.vert");
    m_flatFragment = loadShader("flat.frag");
    m_backgroundVertex = loadShader("background.vert");
    m_backgroundFragment = loadShader("background.frag");
    m_shadowVertex = loadShader("shadow.vert");
    m_shadowFragment = loadShader("shadow.frag");
    m_fullscreenVertex = loadShader("fullscreen.vert");
    m_ssaoFragment = loadShader("ssao.frag");
    m_aoBlurFragment = loadShader("aoblur.frag");
    m_normalFragment = loadShader("normal.frag");
    m_pickVertex = loadShader("pick.vert");
    m_pickCornerVertex = loadShader("pickcorner.vert");
    m_pickFragment = loadShader("pick.frag");
    m_overlayVertex = loadShader("overlay.vert");
    m_overlayFragment = loadShader("overlay.frag");
    createPickResources();

    // Depth only: the position comes from the body layout, there is no colour attachment.
    m_shadowPipeline.reset(m_rhi->newGraphicsPipeline());
    m_shadowPipeline->setShaderStages(
        {{QRhiShaderStage::Vertex, m_shadowVertex}, {QRhiShaderStage::Fragment, m_shadowFragment}});
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(MeshVertex)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});
    m_shadowPipeline->setVertexInputLayout(layout);
    m_shadowPipeline->setDepthTest(true);
    m_shadowPipeline->setDepthWrite(true);
    m_shadowPipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_shadowPipeline->setDepthBias(4);
    m_shadowPipeline->setSlopeScaledDepthBias(2.0F);
    m_shadowPipeline->setTargetBlends({});
    m_shadowPipeline->setShaderResourceBindings(m_shadowSrb.get());
    m_shadowPipeline->setRenderPassDescriptor(m_shadowPass.get());
    m_shadowPipeline->create();
}

SceneRenderer::~SceneRenderer() {
    // A pick still on its way would be answered into this object after it is gone (the graphics
    // interface finishes its readbacks when it shuts down). Take the answer now, and nobody is
    // told.
    if (m_pickInFlight && m_rhi != nullptr && !m_rhi->isRecordingFrame()) {
        m_pickDone = nullptr;
        m_pickResult.completed = nullptr;
        m_rhi->finish();
    }
}

std::unique_ptr<QRhiGraphicsPipeline>
SceneRenderer::makePipeline(const QShader& vertex, const QShader& fragment, bool colorLayoutOnly,
                            bool lines, bool blend, bool depth, QRhiRenderPassDescriptor* pass,
                            int samples, QRhiShaderResourceBindings* srb,
                            QRhiGraphicsPipeline::CompareOp depthOp, bool surfaceBias) {
    std::unique_ptr<QRhiGraphicsPipeline> pipeline(m_rhi->newGraphicsPipeline());
    pipeline->setShaderStages(
        {{QRhiShaderStage::Vertex, vertex}, {QRhiShaderStage::Fragment, fragment}});
    QRhiVertexInputLayout layout;
    if (colorLayoutOnly) {
        layout.setBindings({{sizeof(FlatVertex)}});
        layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                              {0, 1, QRhiVertexInputAttribute::Float4, 3 * sizeof(float)}});
    } else {
        layout.setBindings({{sizeof(MeshVertex)}});
        layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                              {0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)},
                              {0, 2, QRhiVertexInputAttribute::Float4, 6 * sizeof(float)}});
    }
    pipeline->setVertexInputLayout(layout);
    pipeline->setTopology(lines ? QRhiGraphicsPipeline::Lines : QRhiGraphicsPipeline::Triangles);
    pipeline->setDepthTest(depth);
    pipeline->setDepthWrite(depth && !blend);
    pipeline->setDepthOp(depthOp);
    pipeline->setCullMode(QRhiGraphicsPipeline::None);
    if (!lines && depth && surfaceBias) {
        // Surfaces sit a little behind, so that edge lines and the grid win the depth test.
        pipeline->setDepthBias(2);
        pipeline->setSlopeScaledDepthBias(1.0F);
    }
    if (blend) {
        QRhiGraphicsPipeline::TargetBlend blendState;
        blendState.enable = true;
        blendState.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blendState.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blendState.srcAlpha = QRhiGraphicsPipeline::One;
        blendState.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline->setTargetBlends({blendState});
    }
    pipeline->setSampleCount(samples);
    pipeline->setShaderResourceBindings(srb != nullptr ? srb : m_srb.get());
    pipeline->setRenderPassDescriptor(pass);
    pipeline->create();
    return pipeline;
}

void SceneRenderer::createPipelines(QRhiRenderPassDescriptor* pass, int sampleCount) {
    m_backgroundPipeline = makePipeline(m_backgroundVertex, m_backgroundFragment, true, false,
                                        false, false, pass, sampleCount);
    m_bedPipeline =
        makePipeline(m_bodyVertex, m_bedFragment, false, false, false, true, pass, sampleCount);
    m_bodyPipeline =
        makePipeline(m_bodyVertex, m_bodyFragment, false, false, false, true, pass, sampleCount);
    // Holes are drawn see-through, after everything solid, and cast no shadow.
    m_ghostPipeline =
        makePipeline(m_bodyVertex, m_bodyFragment, false, false, true, true, pass, sampleCount);
    m_discPipeline =
        makePipeline(m_flatVertex, m_flatFragment, true, false, true, true, pass, sampleCount);
    m_linePipeline =
        makePipeline(m_flatVertex, m_flatFragment, true, true, true, true, pass, sampleCount);
    // Handles: tested against the depth of the scene but not written into it; the hidden half
    // is drawn where something is in front.
    m_handleHiddenPipeline =
        makePipeline(m_flatVertex, m_flatFragment, true, false, true, true, pass, sampleCount,
                     nullptr, QRhiGraphicsPipeline::Greater, false);
    m_handlePipeline = makePipeline(m_flatVertex, m_flatFragment, true, false, true, true, pass,
                                    sampleCount, nullptr, QRhiGraphicsPipeline::LessOrEqual, false);

    // Highlights of faces: the body's own vertices, one colour, blended over the body.
    m_overlayPipeline.reset(m_rhi->newGraphicsPipeline());
    m_overlayPipeline->setShaderStages({{QRhiShaderStage::Vertex, m_overlayVertex},
                                        {QRhiShaderStage::Fragment, m_overlayFragment}});
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(MeshVertex)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});
    m_overlayPipeline->setVertexInputLayout(layout);
    m_overlayPipeline->setDepthTest(true);
    m_overlayPipeline->setDepthWrite(false);
    m_overlayPipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    m_overlayPipeline->setCullMode(QRhiGraphicsPipeline::None);
    QRhiGraphicsPipeline::TargetBlend blendState;
    blendState.enable = true;
    blendState.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blendState.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blendState.srcAlpha = QRhiGraphicsPipeline::One;
    blendState.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_overlayPipeline->setTargetBlends({blendState});
    m_overlayPipeline->setSampleCount(sampleCount);
    m_overlayPipeline->setShaderResourceBindings(m_overlaySrb.get());
    m_overlayPipeline->setRenderPassDescriptor(pass);
    m_overlayPipeline->create();
}

std::unique_ptr<QRhiBuffer> SceneRenderer::makeVertexBuffer(QRhiResourceUpdateBatch* updates,
                                                            const void* data, std::size_t bytes,
                                                            QRhiBuffer::UsageFlags usage) {
    if (bytes == 0) {
        return nullptr;
    }
    std::unique_ptr<QRhiBuffer> buffer(
        m_rhi->newBuffer(QRhiBuffer::Immutable, usage, static_cast<quint32>(bytes)));
    buffer->create();
    updates->uploadStaticBuffer(buffer.get(), data);
    return buffer;
}

void SceneRenderer::upload(QRhiCommandBuffer* cb, const RenderScene& scene,
                           const ViewPalette& palette) {
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    if (!m_whiteUploaded) {
        const std::uint8_t white[4] = {255, 255, 255, 255};
        QRhiTextureSubresourceUploadDescription pixel(white, 4);
        updates->uploadTexture(m_aoWhite.get(), QRhiTextureUploadEntry(0, 0, pixel));
        m_whiteUploaded = true;
    }

    // --- bodies: buffers per body; only new or changed ones are built and sent ------------
    std::vector<FlatVertex> lines;
    QVector3D boundsMin(1e30F, 1e30F, 1e30F);
    QVector3D boundsMax(-1e30F, -1e30F, -1e30F);
    bool anyBody = false;

    const float hx = 0.5F * scene.bed.sizeX;
    const float hy = 0.5F * scene.bed.sizeY;
    // Axes and the rim of the plate.
    lines.push_back(flatVertex(-hx, 0.0F, 0.0F, palette.axisX, kAxisAlpha));
    lines.push_back(flatVertex(hx, 0.0F, 0.0F, palette.axisX, kAxisAlpha));
    lines.push_back(flatVertex(0.0F, -hy, 0.0F, palette.axisY, kAxisAlpha));
    lines.push_back(flatVertex(0.0F, hy, 0.0F, palette.axisY, kAxisAlpha));

    const std::vector<QPointF> outline = roundedRectangle(scene.bed);
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const QPointF& a = outline[i];
        const QPointF& b = outline[(i + 1) % outline.size()];
        lines.push_back(flatVertex(static_cast<float>(a.x()), static_cast<float>(a.y()), 0.0F,
                                   palette.bedEdge));
        lines.push_back(flatVertex(static_cast<float>(b.x()), static_cast<float>(b.y()), 0.0F,
                                   palette.bedEdge));
    }

    // The edge colour is part of the line buffers: a new colour rebuilds them all.
    const bool edgeColorChanged = palette.edgeLine != m_edgeLineColor;
    m_edgeLineColor = palette.edgeLine;

    // Every body gets a number for the pick picture. Bodies that stay keep theirs; new ones take
    // a free one (the numbers of bodies that are going away are free from the next upload on).
    std::vector<bool> slotUsed(kPickSlots, false);
    slotUsed[0] = true;
    for (const auto& entry : m_gpuBodies) {
        slotUsed[entry.second.slot] = true;
    }
    const auto takeSlot = [&slotUsed]() -> unsigned {
        for (unsigned slot = 1; slot < kPickSlots; ++slot) {
            if (!slotUsed[slot]) {
                slotUsed[slot] = true;
                return slot;
            }
        }
        return 0;
    };

    std::map<std::string, GpuBody> next;
    std::vector<std::string> order;
    m_drawOrder.clear();
    m_lastStats = {};
    std::size_t totalIndices = 0;
    for (std::size_t position = 0; position < scene.bodies.size(); ++position) {
        const RenderBody& body = scene.bodies[position];
        // Bodies without an id (tests, previews) are told apart by their place in the list.
        const std::string key = body.id.empty() ? "#" + std::to_string(position) : body.id;
        if (next.contains(key)) {
            continue; // the same id twice: the first one wins
        }
        GpuBody gpu;
        const auto known = m_gpuBodies.find(key);
        const bool sameMesh = known != m_gpuBodies.end() &&
                              known->second.meshVersion == body.meshVersion &&
                              known->second.color == body.color;
        if (sameMesh && !edgeColorChanged) {
            gpu = std::move(known->second);
            m_gpuBodies.erase(known);
            ++m_lastStats.kept;
        } else {
            const geo::TriangleMesh& mesh = body.geometry();
            std::vector<MeshVertex> vertices;
            std::vector<FlatVertex> edgeLines;
            vertices.reserve(mesh.positions.size());
            gpu.boundsMin = QVector3D(1e30F, 1e30F, 1e30F);
            gpu.boundsMax = QVector3D(-1e30F, -1e30F, -1e30F);
            for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
                const geo::Point3f& p = mesh.positions[i];
                const geo::Point3f& n = mesh.normals[i];
                MeshVertex vertex = meshVertex(p.x, p.y, p.z, n.x, n.y, n.z);
                setColor(vertex.color, body.color);
                vertices.push_back(vertex);
                gpu.boundsMin =
                    QVector3D(std::min(gpu.boundsMin.x(), p.x), std::min(gpu.boundsMin.y(), p.y),
                              std::min(gpu.boundsMin.z(), p.z));
                gpu.boundsMax =
                    QVector3D(std::max(gpu.boundsMax.x(), p.x), std::max(gpu.boundsMax.y(), p.y),
                              std::max(gpu.boundsMax.z(), p.z));
            }
            for (const geo::EdgePolyline& edge : mesh.edges) {
                for (std::size_t i = 0; i + 1 < edge.points.size(); ++i) {
                    const geo::Point3f& a = edge.points[i];
                    const geo::Point3f& b = edge.points[i + 1];
                    edgeLines.push_back(flatVertex(a.x, a.y, a.z, palette.edgeLine));
                    edgeLines.push_back(flatVertex(b.x, b.y, b.z, palette.edgeLine));
                }
            }
            gpu.meshVersion = body.meshVersion;
            gpu.color = body.color;
            gpu.mesh = body.mesh;
            gpu.slot = known != m_gpuBodies.end() && known->second.slot != 0 ? known->second.slot
                                                                             : takeSlot();
            if (gpu.slot != 0) {
                buildPickBuffers(updates, mesh, gpu);
            }
            gpu.vertices =
                makeVertexBuffer(updates, vertices.data(), vertices.size() * sizeof(MeshVertex),
                                 QRhiBuffer::VertexBuffer);
            gpu.indices = makeVertexBuffer(updates, mesh.indices.data(),
                                           mesh.indices.size() * sizeof(std::uint32_t),
                                           QRhiBuffer::IndexBuffer);
            gpu.indexCount = mesh.indices.size();
            gpu.lines =
                makeVertexBuffer(updates, edgeLines.data(), edgeLines.size() * sizeof(FlatVertex),
                                 QRhiBuffer::VertexBuffer);
            gpu.lineVertexCount = edgeLines.size();
            ++m_lastStats.uploaded;
        }
        if (gpu.vertices) {
            boundsMin = QVector3D(std::min(boundsMin.x(), gpu.boundsMin.x()),
                                  std::min(boundsMin.y(), gpu.boundsMin.y()),
                                  std::min(boundsMin.z(), gpu.boundsMin.z()));
            boundsMax = QVector3D(std::max(boundsMax.x(), gpu.boundsMax.x()),
                                  std::max(boundsMax.y(), gpu.boundsMax.y()),
                                  std::max(boundsMax.z(), gpu.boundsMax.z()));
            anyBody = true;
        }
        totalIndices += gpu.indexCount;
        next.emplace(key, std::move(gpu));
        order.push_back(key);
    }
    m_lastStats.removed = static_cast<int>(m_gpuBodies.size());
    m_gpuBodies = std::move(next); // what is left in the old map is gone: its buffers are freed
    m_slotKeys.clear();
    for (const auto& entry : m_gpuBodies) {
        if (entry.second.slot != 0) {
            m_slotKeys[entry.second.slot] = entry.first;
        }
    }
    // Drawing follows the order of the scene.
    for (const std::string& key : order) {
        m_drawOrder.push_back(&m_gpuBodies.at(key));
    }
    m_bodyIndexCount = totalIndices;
    m_lineVertexCount = lines.size();
    m_lineVertices = makeVertexBuffer(updates, lines.data(), lines.size() * sizeof(FlatVertex),
                                      QRhiBuffer::VertexBuffer);

    // --- the plate: top as a fan around the origin, edge as a strip -------------------------
    std::vector<MeshVertex> bedVertices;
    std::vector<quint32> bedIndices;
    bedVertices.push_back(meshVertex(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F));
    for (const QPointF& p : outline) {
        bedVertices.push_back(meshVertex(static_cast<float>(p.x()), static_cast<float>(p.y()), 0.0F,
                                         0.0F, 0.0F, 1.0F));
    }
    const auto count = static_cast<quint32>(outline.size());
    for (quint32 i = 0; i < count; ++i) {
        bedIndices.insert(bedIndices.end(), {0, 1 + i, 1 + (i + 1) % count});
    }
    const float thickness = scene.bed.thickness;
    for (quint32 i = 0; i < count; ++i) {
        const QPointF& a = outline[i];
        const QPointF& b = outline[(i + 1) % count];
        // outward normal of the segment (outline runs counter-clockwise)
        float nx = static_cast<float>(b.y() - a.y());
        float ny = static_cast<float>(a.x() - b.x());
        const float length = std::hypot(nx, ny);
        nx /= length > 0.0F ? length : 1.0F;
        ny /= length > 0.0F ? length : 1.0F;
        const auto first = static_cast<quint32>(bedVertices.size());
        const auto ax = static_cast<float>(a.x());
        const auto ay = static_cast<float>(a.y());
        const auto bx = static_cast<float>(b.x());
        const auto by = static_cast<float>(b.y());
        bedVertices.push_back(meshVertex(ax, ay, 0.0F, nx, ny, 0.0F));
        bedVertices.push_back(meshVertex(bx, by, 0.0F, nx, ny, 0.0F));
        bedVertices.push_back(meshVertex(bx, by, -thickness, nx, ny, 0.0F));
        bedVertices.push_back(meshVertex(ax, ay, -thickness, nx, ny, 0.0F));
        bedIndices.insert(bedIndices.end(),
                          {first, first + 1, first + 2, first, first + 2, first + 3});
    }
    m_bedVertices =
        makeVertexBuffer(updates, bedVertices.data(), bedVertices.size() * sizeof(MeshVertex),
                         QRhiBuffer::VertexBuffer);
    m_bedIndices = makeVertexBuffer(updates, bedIndices.data(), bedIndices.size() * sizeof(quint32),
                                    QRhiBuffer::IndexBuffer);
    m_bedIndexCount = bedIndices.size();

    // --- origin marker: a small disc on the plate -------------------------------------------
    std::vector<FlatVertex> disc;
    constexpr int kDiscSegments = 24;
    constexpr float kDiscRadius = 1.3F;
    for (int i = 0; i < kDiscSegments; ++i) {
        const float a0 = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) / kDiscSegments;
        const float a1 =
            2.0F * std::numbers::pi_v<float> * static_cast<float>(i + 1) / kDiscSegments;
        disc.push_back(flatVertex(0.0F, 0.0F, 0.0F, palette.origin));
        disc.push_back(flatVertex(kDiscRadius * std::cos(a0), kDiscRadius * std::sin(a0), 0.0F,
                                  palette.origin));
        disc.push_back(flatVertex(kDiscRadius * std::cos(a1), kDiscRadius * std::sin(a1), 0.0F,
                                  palette.origin));
    }
    m_discVertices = makeVertexBuffer(updates, disc.data(), disc.size() * sizeof(FlatVertex),
                                      QRhiBuffer::VertexBuffer);
    m_discVertexCount = disc.size();

    // --- light: orthographic shadow frustum around the bodies ------------------------------
    m_hasShadow = anyBody;
    // How far corners reach into their surroundings follows the size of the scene.
    m_aoRadius = anyBody ? std::clamp(0.15F * (boundsMax - boundsMin).length(), 2.0F, 25.0F) : 6.0F;
    m_lightDirection = QVector3D(-0.4F, -0.5F, 0.8F).normalized();
    if (anyBody) {
        const QVector3D center = 0.5F * (boundsMin + boundsMax);
        const float radius = 0.5F * (boundsMax - boundsMin).length() * 1.6F + 5.0F;
        QMatrix4x4 lightView;
        lightView.lookAt(center + m_lightDirection * radius * 2.0F, center, QVector3D(0, 0, 1));
        QMatrix4x4 lightProjection;
        lightProjection.ortho(-radius, radius, -radius, radius, radius * 0.2F, radius * 4.0F);
        m_lightViewProjection = lightProjection * lightView;
    }
    cb->resourceUpdate(updates);

    if (!m_hasShadow || m_drawOrder.empty()) {
        return;
    }
    // Shadow map: depth of the bodies seen from the light.
    QRhiResourceUpdateBatch* shadowUpdates = m_rhi->nextResourceUpdateBatch();
    Frame none;
    none.palette = palette;
    writeUniforms(shadowUpdates, m_shadowUniforms.get(),
                  m_rhi->clipSpaceCorrMatrix() * m_lightViewProjection, none);
    cb->beginPass(m_shadowTarget.get(), QColor(0, 0, 0, 255), {1.0F, 0}, shadowUpdates);
    cb->setGraphicsPipeline(m_shadowPipeline.get());
    cb->setViewport({0, 0, float(kShadowSize), float(kShadowSize)});
    cb->setShaderResources(m_shadowSrb.get());
    for (const GpuBody* body : m_drawOrder) {
        if (!body->vertices || !body->indices || isGhost(*body)) {
            continue;
        }
        const QRhiCommandBuffer::VertexInput input(body->vertices.get(), 0);
        cb->setVertexInput(0, 1, &input, body->indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(static_cast<quint32>(body->indexCount));
    }
    cb->endPass();
}

void SceneRenderer::writeUniforms(QRhiResourceUpdateBatch* updates, QRhiBuffer* buffer,
                                  const QMatrix4x4& mvp, const Frame& frame,
                                  const QSize& targetSize, bool occlusion) const {
    Params p{};
    p.view[0] = static_cast<float>(targetSize.width());
    p.view[1] = static_cast<float>(targetSize.height());
    p.view[2] = occlusion ? 1.0F : 0.0F;
    copyMatrix(p.mvp, mvp);
    copyMatrix(p.lightVp, m_lightViewProjection);
    copyMatrix(p.corr, m_rhi->clipSpaceCorrMatrix());
    const QVector3D eye = frame.view.inverted().map(QVector3D(0, 0, 0));
    p.eye[0] = eye.x();
    p.eye[1] = eye.y();
    p.eye[2] = eye.z();
    p.lightDir[0] = m_lightDirection.x();
    p.lightDir[1] = m_lightDirection.y();
    p.lightDir[2] = m_lightDirection.z();
    setColor(p.bedColor, frame.palette.bed);
    setColor(p.gridColor, frame.palette.grid);
    setColor(p.gridMajor, frame.palette.gridMajor);
    setColor(p.bgCenter, frame.palette.viewCenter);
    setColor(p.bgEdge, frame.palette.viewEdge);
    setColor(p.shadowColor, frame.palette.shadow);
    // Grid like the mockup: 5 mm lines when zoomed in far, otherwise 10 mm.
    const bool fine = frame.pixelsPerMillimetre > 5.0F;
    p.params[0] = m_rhi->isYUpInFramebuffer() ? 0.0F : 1.0F;
    p.params[1] = 1.0F / float(kShadowSize);
    p.params[2] = fine ? 5.0F : 10.0F;
    p.params[3] = fine ? 10.0F : 50.0F;
    // Wireframe draws no surfaces, so there is nothing that casts a shadow.
    p.bedSize[2] = m_hasShadow && frame.displayMode != DisplayMode::wireframe ? 1.0F : 0.0F;
    if (frame.displayMode == DisplayMode::solid) {
        // "Solid": one neutral colour for all bodies, the colours of the bodies are off.
        p.bodyOverride[0] = 0.84F;
        p.bodyOverride[1] = 0.86F;
        p.bodyOverride[2] = 0.89F;
        p.bodyOverride[3] = 1.0F;
    }
    updates->updateDynamicBuffer(buffer, 0, sizeof(Params), &p);
}

bool SceneRenderer::ensureAmbientOcclusionTargets(const QSize& size) {
    if (size.isEmpty()) {
        return false;
    }
    if (m_aoSize == size && m_aoBlurTarget) {
        return true;
    }
    const QSize half(std::max(1, size.width() / 2), std::max(1, size.height() / 2));
    const auto stages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    const auto fragment = QRhiShaderResourceBinding::FragmentStage;

    // The pipelines and buffers do not depend on the size and are made once.
    if (!m_aoDepthUniforms) {
        m_aoDepthUniforms.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Params)));
        m_aoDepthUniforms->create();
        m_aoRawUniforms.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(AoParams)));
        m_aoRawUniforms->create();
        m_aoBlurUniforms.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(AoParams)));
        m_aoBlurUniforms->create();
        m_aoDepthSrb.reset(m_rhi->newShaderResourceBindings());
        m_aoDepthSrb->setBindings(
            {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_aoDepthUniforms.get())});
        m_aoDepthSrb->create();
    }

    // Depth of the scene as seen from the camera, with the surface normals as colour.
    m_aoDepthTexture.reset(
        m_rhi->newTexture(QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget));
    m_aoDepthTexture->create();
    m_aoNormalTexture.reset(
        m_rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
    m_aoNormalTexture->create();
    QRhiTextureRenderTargetDescription depthDescription(
        QRhiColorAttachment(m_aoNormalTexture.get()));
    depthDescription.setDepthTexture(m_aoDepthTexture.get());
    m_aoDepthTarget.reset(m_rhi->newTextureRenderTarget(depthDescription));
    if (!m_aoDepthPass) {
        m_aoDepthPass.reset(m_aoDepthTarget->newCompatibleRenderPassDescriptor());
    }
    m_aoDepthTarget->setRenderPassDescriptor(m_aoDepthPass.get());
    m_aoDepthTarget->create();
    if (!m_aoDepthPipeline) {
        m_aoDepthPipeline = makePipeline(m_bodyVertex, m_normalFragment, false, false, false, true,
                                         m_aoDepthPass.get(), 1, m_aoDepthSrb.get());
    }

    m_aoRawTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, half, 1, QRhiTexture::RenderTarget));
    m_aoRawTexture->create();
    m_aoBlurTexture.reset(
        m_rhi->newTexture(QRhiTexture::RGBA8, half, 1, QRhiTexture::RenderTarget));
    m_aoBlurTexture->create();
    m_aoRawTarget.reset(m_rhi->newTextureRenderTarget({m_aoRawTexture.get()}));
    if (!m_aoColorPass) {
        m_aoColorPass.reset(m_aoRawTarget->newCompatibleRenderPassDescriptor());
    }
    m_aoRawTarget->setRenderPassDescriptor(m_aoColorPass.get());
    m_aoRawTarget->create();
    m_aoBlurTarget.reset(m_rhi->newTextureRenderTarget({m_aoBlurTexture.get()}));
    m_aoBlurTarget->setRenderPassDescriptor(m_aoColorPass.get());
    m_aoBlurTarget->create();

    m_aoRawSrb.reset(m_rhi->newShaderResourceBindings());
    m_aoRawSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_aoRawUniforms.get()),
         QRhiShaderResourceBinding::sampledTexture(1, fragment, m_aoDepthTexture.get(),
                                                   m_sampler.get()),
         QRhiShaderResourceBinding::sampledTexture(2, fragment, m_aoNormalTexture.get(),
                                                   m_sampler.get())});
    m_aoRawSrb->create();
    m_aoBlurSrb.reset(m_rhi->newShaderResourceBindings());
    m_aoBlurSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_aoBlurUniforms.get()),
         QRhiShaderResourceBinding::sampledTexture(1, fragment, m_aoRawTexture.get(),
                                                   m_linearSampler.get())});
    m_aoBlurSrb->create();

    const auto makeFullscreen = [&](const QShader& fragmentShader,
                                    QRhiShaderResourceBindings* srb) {
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(m_rhi->newGraphicsPipeline());
        pipeline->setShaderStages({{QRhiShaderStage::Vertex, m_fullscreenVertex},
                                   {QRhiShaderStage::Fragment, fragmentShader}});
        QRhiVertexInputLayout empty;
        pipeline->setVertexInputLayout(empty);
        pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        pipeline->setCullMode(QRhiGraphicsPipeline::None);
        pipeline->setShaderResourceBindings(srb);
        pipeline->setRenderPassDescriptor(m_aoColorPass.get());
        pipeline->create();
        return pipeline;
    };
    if (!m_aoRawPipeline) {
        m_aoRawPipeline = makeFullscreen(m_ssaoFragment, m_aoRawSrb.get());
        m_aoBlurPipeline = makeFullscreen(m_aoBlurFragment, m_aoBlurSrb.get());
    }

    // The shaders of the main pass read the finished occlusion image.
    m_srb->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, stages, m_uniforms.get()),
                        QRhiShaderResourceBinding::sampledTexture(
                            1, fragment, m_shadowTexture.get(), m_sampler.get()),
                        QRhiShaderResourceBinding::sampledTexture(
                            2, fragment, m_aoBlurTexture.get(), m_linearSampler.get())});
    m_srb->create();
    m_aoSize = size;
    return true;
}

void SceneRenderer::renderAmbientOcclusion(QRhiCommandBuffer* cb, const Frame& frame,
                                           const QSize& size) {
    const QSize half(std::max(1, size.width() / 2), std::max(1, size.height() / 2));
    const QMatrix4x4 viewProjection = frame.projection * frame.view;
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();

    writeUniforms(updates, m_aoDepthUniforms.get(), m_rhi->clipSpaceCorrMatrix() * viewProjection,
                  frame, size, false);

    // pixels per mm at distance 1 (perspective) or everywhere (orthographic)
    const float scale = 0.5F * static_cast<float>(size.height()) * frame.projection(1, 1);
    const bool orthographic = frame.projection(3, 3) > 0.5F;
    const QVector3D eye = frame.view.inverted().map(QVector3D(0, 0, 0));
    const auto fill = [&](QRhiBuffer* buffer, const QSize& image) {
        AoParams p{};
        copyMatrix(p.invViewProj, viewProjection.inverted());
        copyMatrix(p.corr, m_rhi->clipSpaceCorrMatrix());
        p.eye[0] = eye.x();
        p.eye[1] = eye.y();
        p.eye[2] = eye.z();
        p.a[0] = m_rhi->isYUpInFramebuffer() ? 0.0F : 1.0F;
        p.a[1] = m_aoRadius;
        p.a[2] = orthographic ? 0.0F : scale;
        p.a[3] = orthographic ? scale : 0.0F;
        p.b[0] = static_cast<float>(image.width());
        p.b[1] = static_cast<float>(image.height());
        p.b[2] = 0.85F; // strength
        updates->updateDynamicBuffer(buffer, 0, sizeof(AoParams), &p);
    };
    fill(m_aoRawUniforms.get(), size);  // the depth image is read at full size
    fill(m_aoBlurUniforms.get(), half); // the blur reads texels of the half-size image

    // 1. depth of the bodies and the plate, from the camera
    cb->beginPass(m_aoDepthTarget.get(), QColor(128, 128, 255, 255), {1.0F, 0}, updates);
    cb->setGraphicsPipeline(m_aoDepthPipeline.get());
    cb->setViewport({0, 0, float(size.width()), float(size.height())});
    cb->setShaderResources(m_aoDepthSrb.get());
    for (const GpuBody* body : m_drawOrder) {
        if (!body->vertices || !body->indices || isGhost(*body)) {
            continue;
        }
        const QRhiCommandBuffer::VertexInput input(body->vertices.get(), 0);
        cb->setVertexInput(0, 1, &input, body->indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(static_cast<quint32>(body->indexCount));
    }
    if (m_bedVertices && m_bedIndices) {
        const QRhiCommandBuffer::VertexInput input(m_bedVertices.get(), 0);
        cb->setVertexInput(0, 1, &input, m_bedIndices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(static_cast<quint32>(m_bedIndexCount));
    }
    cb->endPass();

    // 2. occlusion from the depth, 3. blur
    cb->beginPass(m_aoRawTarget.get(), QColor(255, 255, 255, 255), {1.0F, 0});
    cb->setGraphicsPipeline(m_aoRawPipeline.get());
    cb->setViewport({0, 0, float(half.width()), float(half.height())});
    cb->setShaderResources(m_aoRawSrb.get());
    cb->draw(3);
    cb->endPass();

    cb->beginPass(m_aoBlurTarget.get(), QColor(255, 255, 255, 255), {1.0F, 0});
    cb->setGraphicsPipeline(m_aoBlurPipeline.get());
    cb->setViewport({0, 0, float(half.width()), float(half.height())});
    cb->setShaderResources(m_aoBlurSrb.get());
    cb->draw(3);
    cb->endPass();
}

void SceneRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderTarget* target, const Frame& frame) {
    if (!m_bodyPipeline) {
        return;
    }
    const QSize size = target->pixelSize();
    // Without bodies there is nothing that could be in a corner.
    const bool occlusion = frame.ambientOcclusion && !m_drawOrder.empty() &&
                           frame.displayMode != DisplayMode::wireframe &&
                           ensureAmbientOcclusionTargets(size);
    if (occlusion) {
        renderAmbientOcclusion(cb, frame, size);
    }
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    writeUniforms(updates, m_uniforms.get(),
                  m_rhi->clipSpaceCorrMatrix() * frame.projection * frame.view, frame, size,
                  occlusion);
    prepareHighlights(updates, frame, size);
    prepareHandles(updates, frame, size);

    cb->beginPass(target, frame.palette.viewEdge, {1.0F, 0}, updates);
    cb->setViewport({0, 0, float(size.width()), float(size.height())});

    cb->setGraphicsPipeline(m_backgroundPipeline.get());
    cb->setShaderResources(m_srb.get());
    cb->draw(3);

    if (frame.bedVisible && m_bedVertices) {
        const QRhiCommandBuffer::VertexInput input(m_bedVertices.get(), 0);
        cb->setGraphicsPipeline(m_bedPipeline.get());
        cb->setShaderResources(m_srb.get());
        cb->setVertexInput(0, 1, &input, m_bedIndices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(static_cast<quint32>(m_bedIndexCount));
    }
    if (frame.displayMode != DisplayMode::wireframe) {
        // Solid bodies first, then the see-through ones over them.
        for (const bool ghosts : {false, true}) {
            cb->setGraphicsPipeline(ghosts ? m_ghostPipeline.get() : m_bodyPipeline.get());
            cb->setShaderResources(m_srb.get());
            for (const GpuBody* body : m_drawOrder) {
                if (!body->vertices || !body->indices || isGhost(*body) != ghosts) {
                    continue;
                }
                const QRhiCommandBuffer::VertexInput input(body->vertices.get(), 0);
                cb->setVertexInput(0, 1, &input, body->indices.get(), 0,
                                   QRhiCommandBuffer::IndexUInt32);
                cb->drawIndexed(static_cast<quint32>(body->indexCount));
            }
        }
    }
    if (frame.bedVisible && m_discVertices) {
        const QRhiCommandBuffer::VertexInput input(m_discVertices.get(), 0);
        cb->setGraphicsPipeline(m_discPipeline.get());
        cb->setShaderResources(m_srb.get());
        cb->setVertexInput(0, 1, &input);
        cb->draw(static_cast<quint32>(m_discVertexCount));
    }
    if (m_lineVertices || !m_drawOrder.empty()) {
        cb->setGraphicsPipeline(m_linePipeline.get());
        cb->setShaderResources(m_srb.get());
        // Axes and the plate's rim belong to the bed, then come the bodies' edges.
        if (m_lineVertices) {
            const QRhiCommandBuffer::VertexInput input(m_lineVertices.get(), 0);
            cb->setVertexInput(0, 1, &input);
            cb->draw(static_cast<quint32>(m_lineVertexCount));
        }
        for (const GpuBody* body : m_drawOrder) {
            if (!body->lines || body->lineVertexCount == 0) {
                continue;
            }
            const QRhiCommandBuffer::VertexInput input(body->lines.get(), 0);
            cb->setVertexInput(0, 1, &input);
            cb->draw(static_cast<quint32>(body->lineVertexCount));
        }
    }
    drawHighlights(cb);
    drawHandles(cb);
    cb->endPass();
}

// --- picking -------------------------------------------------------------------------------

void SceneRenderer::createPickResources() {
    const QSize size(kPickSize, kPickSize);
    m_pickTexture.reset(
        m_rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    m_pickTexture->create();
    m_pickDepth.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    m_pickDepth->create();
    QRhiTextureRenderTargetDescription description{QRhiColorAttachment(m_pickTexture.get())};
    description.setDepthStencilBuffer(m_pickDepth.get());
    m_pickTarget.reset(m_rhi->newTextureRenderTarget(description));
    m_pickPass.reset(m_pickTarget->newCompatibleRenderPassDescriptor());
    m_pickTarget->setRenderPassDescriptor(m_pickPass.get());
    m_pickTarget->create();

    m_pickUniforms.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(PickParams)));
    m_pickUniforms->create();
    m_pickSrb.reset(m_rhi->newShaderResourceBindings());
    m_pickSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage, m_pickUniforms.get())});
    m_pickSrb->create();

    const auto make = [this](const QShader& vertex, QRhiVertexInputLayout layout,
                             QRhiGraphicsPipeline::Topology topology, bool surfaceBias) {
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(m_rhi->newGraphicsPipeline());
        pipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, vertex}, {QRhiShaderStage::Fragment, m_pickFragment}});
        pipeline->setVertexInputLayout(layout);
        pipeline->setTopology(topology);
        pipeline->setDepthTest(true);
        pipeline->setDepthWrite(true);
        pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        pipeline->setCullMode(QRhiGraphicsPipeline::None);
        if (surfaceBias) {
            // As in the view: surfaces sit a little behind, so edges and corners win.
            pipeline->setDepthBias(2);
            pipeline->setSlopeScaledDepthBias(1.0F);
        }
        pipeline->setShaderResourceBindings(m_pickSrb.get());
        pipeline->setRenderPassDescriptor(m_pickPass.get());
        pipeline->create();
        return pipeline;
    };

    QRhiVertexInputLayout faces;
    faces.setBindings({{sizeof(MeshVertex)}, {sizeof(std::uint32_t)}});
    faces.setAttributes(
        {{0, 0, QRhiVertexInputAttribute::Float3, 0}, {1, 1, QRhiVertexInputAttribute::UInt, 0}});
    m_pickFacePipeline = make(m_pickVertex, faces, QRhiGraphicsPipeline::Triangles, true);

    QRhiVertexInputLayout lines;
    lines.setBindings({{sizeof(PickVertex)}});
    lines.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                         {0, 1, QRhiVertexInputAttribute::UInt, 3 * sizeof(float)}});
    m_pickEdgePipeline = make(m_pickVertex, lines, QRhiGraphicsPipeline::Lines, false);

    QRhiVertexInputLayout corners;
    corners.setBindings({{sizeof(PickCornerVertex)}});
    corners.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                           {0, 1, QRhiVertexInputAttribute::UInt, 3 * sizeof(float)},
                           {0, 2, QRhiVertexInputAttribute::Float2, 4 * sizeof(float)}});
    m_pickCornerPipeline =
        make(m_pickCornerVertex, corners, QRhiGraphicsPipeline::Triangles, false);

    // The highlights' colours: four styles in one buffer, each at the alignment the graphics
    // interface asks for.
    m_overlayStride = static_cast<quint32>(m_rhi->ubufAligned(sizeof(OverlayParams)));
    m_overlayUniforms.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                             m_overlayStride * kOverlayStyles));
    m_overlayUniforms->create();
    m_overlaySrb.reset(m_rhi->newShaderResourceBindings());
    m_overlaySrb->setBindings({QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_overlayUniforms.get(), sizeof(OverlayParams))});
    m_overlaySrb->create();
}

void SceneRenderer::buildPickBuffers(QRhiResourceUpdateBatch* updates,
                                     const geo::TriangleMesh& mesh, GpuBody& body) {
    // Every face has its own vertices, so a number per vertex says which face it is.
    std::vector<std::uint32_t> numbers(mesh.positions.size(), 0);
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const std::uint32_t face = t < mesh.faceIds.size() ? mesh.faceIds[t] : 0;
        const std::uint32_t id = packPickId(core::SelectableKind::face, body.slot, face);
        for (std::size_t k = 0; k < 3; ++k) {
            numbers[mesh.indices[3 * t + k]] = id;
        }
    }
    body.faceNumbers = makeVertexBuffer(
        updates, numbers.data(), numbers.size() * sizeof(std::uint32_t), QRhiBuffer::VertexBuffer);

    // Where the triangles of each face are, for highlighting one face.
    body.faceRuns.clear();
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const std::uint32_t face = t < mesh.faceIds.size() ? mesh.faceIds[t] : 0;
        if (!body.faceRuns.empty() && body.faceRuns.back().face == face &&
            body.faceRuns.back().firstTriangle + body.faceRuns.back().triangleCount == t) {
            ++body.faceRuns.back().triangleCount;
        } else {
            body.faceRuns.push_back({face, static_cast<std::uint32_t>(t), 1});
        }
    }
    std::sort(body.faceRuns.begin(), body.faceRuns.end(), [](const auto& a, const auto& b) {
        return a.face != b.face ? a.face < b.face : a.firstTriangle < b.firstTriangle;
    });

    std::vector<PickVertex> lines;
    for (const geo::EdgePolyline& edge : mesh.edges) {
        const std::uint32_t id = packPickId(core::SelectableKind::edge, body.slot, edge.edgeId);
        for (std::size_t i = 0; i + 1 < edge.points.size(); ++i) {
            for (const geo::Point3f* p : {&edge.points[i], &edge.points[i + 1]}) {
                lines.push_back({{p->x, p->y, p->z}, id});
            }
        }
    }
    body.edgePick = makeVertexBuffer(updates, lines.data(), lines.size() * sizeof(PickVertex),
                                     QRhiBuffer::VertexBuffer);
    body.edgePickCount = lines.size();

    std::vector<PickCornerVertex> squares;
    constexpr float h = kCornerHalfSize;
    constexpr float corners[6][2] = {{-h, -h}, {h, -h}, {h, h}, {-h, -h}, {h, h}, {-h, h}};
    for (const geo::CornerPoint& corner : mesh.corners) {
        const std::uint32_t id =
            packPickId(core::SelectableKind::vertex, body.slot, corner.cornerId);
        for (const auto& offset : corners) {
            squares.push_back({{corner.position.x, corner.position.y, corner.position.z},
                               id,
                               {offset[0], offset[1]}});
        }
    }
    body.cornerPick =
        makeVertexBuffer(updates, squares.data(), squares.size() * sizeof(PickCornerVertex),
                         QRhiBuffer::VertexBuffer);
    body.cornerPickCount = squares.size();
}

std::string SceneRenderer::bodyOfSlot(unsigned slot) const {
    const auto found = m_slotKeys.find(slot);
    return found == m_slotKeys.end() ? std::string{} : found->second;
}

unsigned SceneRenderer::slotOfBody(const std::string& id) const {
    const auto found = m_gpuBodies.find(id);
    return found == m_gpuBodies.end() ? 0U : found->second.slot;
}

bool SceneRenderer::pick(QRhiCommandBuffer* cb, const QSize& targetSize, const QPoint& pixel,
                         const QMatrix4x4& view, const QMatrix4x4& projection,
                         const core::SelectionFilter& filter, PickCallback done) {
    const QRect window(pixel.x() - kPickRadius, pixel.y() - kPickRadius, kPickSize, kPickSize);
    return pickPass(cb, targetSize, window, view, projection, filter, std::move(done),
                    m_pickTarget.get(), m_pickTexture.get());
}

bool SceneRenderer::pickRegion(QRhiCommandBuffer* cb, const QSize& targetSize, const QRect& area,
                               const QMatrix4x4& view, const QMatrix4x4& projection,
                               const core::SelectionFilter& filter, PickCallback done) {
    const QRect inside = area.intersected(QRect(QPoint(0, 0), targetSize));
    if (inside.width() < 1 || inside.height() < 1 || inside.width() > kRegionMaxSize ||
        inside.height() > kRegionMaxSize) {
        return false;
    }
    // The picture of a region has the size of the region: a target of that size is made for it.
    if (!m_regionTexture || m_regionTexture->pixelSize() != inside.size()) {
        m_regionTexture.reset(
            m_rhi->newTexture(QRhiTexture::RGBA8, inside.size(), 1,
                              QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        m_regionTexture->create();
        m_regionDepth.reset(
            m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, inside.size(), 1));
        m_regionDepth->create();
        QRhiTextureRenderTargetDescription description{QRhiColorAttachment(m_regionTexture.get())};
        description.setDepthStencilBuffer(m_regionDepth.get());
        m_regionTarget.reset(m_rhi->newTextureRenderTarget(description));
        m_regionTarget->setRenderPassDescriptor(m_pickPass.get()); // same formats: compatible
        m_regionTarget->create();
    }
    return pickPass(cb, targetSize, inside, view, projection, filter, std::move(done),
                    m_regionTarget.get(), m_regionTexture.get());
}

bool SceneRenderer::pickPass(QRhiCommandBuffer* cb, const QSize& targetSize, const QRect& window,
                             const QMatrix4x4& view, const QMatrix4x4& projection,
                             const core::SelectionFilter& filter, PickCallback done,
                             QRhiTextureRenderTarget* target, QRhiTexture* texture) {
    if (m_pickInFlight || m_drawOrder.empty() || targetSize.isEmpty()) {
        return false;
    }
    const QSize size = window.size();
    // Crop the projection so that the pixels of `window` fill the picture.
    const float width = static_cast<float>(targetSize.width());
    const float height = static_cast<float>(targetSize.height());
    const float centerX = static_cast<float>(window.x()) + 0.5F * static_cast<float>(size.width());
    const float centerY = static_cast<float>(window.y()) + 0.5F * static_cast<float>(size.height());
    const float nx = 2.0F * centerX / width - 1.0F;
    const float ny = 1.0F - 2.0F * centerY / height;
    const float scaleX = width / static_cast<float>(size.width());
    const float scaleY = height / static_cast<float>(size.height());
    QMatrix4x4 crop;
    crop(0, 0) = scaleX;
    crop(1, 1) = scaleY;
    crop(0, 3) = -nx * scaleX;
    crop(1, 3) = -ny * scaleY;

    PickParams params{};
    copyMatrix(params.mvp, m_rhi->clipSpaceCorrMatrix() * crop * projection * view);
    params.misc[0] = 2.0F / static_cast<float>(size.width());
    params.misc[1] = 2.0F / static_cast<float>(size.height());
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    updates->updateDynamicBuffer(m_pickUniforms.get(), 0, sizeof(PickParams), &params);

    cb->beginPass(target, QColor(0, 0, 0, 0), {1.0F, 0}, updates);
    cb->setViewport({0, 0, static_cast<float>(size.width()), static_cast<float>(size.height())});

    cb->setGraphicsPipeline(m_pickFacePipeline.get());
    cb->setShaderResources(m_pickSrb.get());
    for (const GpuBody* body : m_drawOrder) {
        if (body->slot == 0 || !body->vertices || !body->indices || !body->faceNumbers) {
            continue;
        }
        const QRhiCommandBuffer::VertexInput inputs[] = {{body->vertices.get(), 0},
                                                         {body->faceNumbers.get(), 0}};
        cb->setVertexInput(0, 2, inputs, body->indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(static_cast<quint32>(body->indexCount));
    }
    // Edges and corners only where they can be chosen: otherwise they would hide the face.
    if (filter.edges) {
        cb->setGraphicsPipeline(m_pickEdgePipeline.get());
        cb->setShaderResources(m_pickSrb.get());
        for (const GpuBody* body : m_drawOrder) {
            if (body->slot == 0 || !body->edgePick || body->edgePickCount == 0) {
                continue;
            }
            const QRhiCommandBuffer::VertexInput input(body->edgePick.get(), 0);
            cb->setVertexInput(0, 1, &input);
            cb->draw(static_cast<quint32>(body->edgePickCount));
        }
    }
    if (filter.vertices) {
        cb->setGraphicsPipeline(m_pickCornerPipeline.get());
        cb->setShaderResources(m_pickSrb.get());
        for (const GpuBody* body : m_drawOrder) {
            if (body->slot == 0 || !body->cornerPick || body->cornerPickCount == 0) {
                continue;
            }
            const QRhiCommandBuffer::VertexInput input(body->cornerPick.get(), 0);
            cb->setVertexInput(0, 1, &input);
            cb->draw(static_cast<quint32>(body->cornerPickCount));
        }
    }

    m_pickDone = std::move(done);
    m_pickResult = {};
    m_pickStarted = std::chrono::steady_clock::now();
    m_pickResult.completed = [this, size] {
        m_lastPickMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                 m_pickStarted)
                           .count();
        const std::size_t count = static_cast<std::size_t>(size.width()) * size.height();
        std::vector<std::uint32_t> picture(count, 0);
        if (m_pickResult.data.size() >= static_cast<qsizetype>(count * sizeof(std::uint32_t))) {
            std::memcpy(picture.data(), m_pickResult.data.constData(),
                        count * sizeof(std::uint32_t));
        }
        if (m_rhi->isYUpInFramebuffer()) {
            // The first row of the data is the bottom one.
            const auto rows = static_cast<std::size_t>(size.height());
            const auto columns = static_cast<std::size_t>(size.width());
            for (std::size_t row = 0; row < rows / 2; ++row) {
                std::swap_ranges(picture.begin() + static_cast<std::ptrdiff_t>(row * columns),
                                 picture.begin() + static_cast<std::ptrdiff_t>((row + 1) * columns),
                                 picture.begin() +
                                     static_cast<std::ptrdiff_t>((rows - 1 - row) * columns));
            }
        }
        m_pickInFlight = false;
        if (m_pickDone) {
            PickCallback finished = std::move(m_pickDone);
            m_pickDone = nullptr;
            finished(std::move(picture));
        }
    };
    QRhiResourceUpdateBatch* readback = m_rhi->nextResourceUpdateBatch();
    readback->readBackTexture(QRhiReadbackDescription(texture), &m_pickResult);
    m_pickInFlight = true;
    cb->endPass(readback);
    return true;
}

// --- highlights ------------------------------------------------------------------------------

namespace {

// Camera-facing strips and squares for edges and corners: lines are one pixel wide on every
// graphics interface, so a highlighted edge is built from triangles of a width in pixels.
class HighlightShapes {
public:
    HighlightShapes(const QMatrix4x4& view, const QMatrix4x4& projection, int heightInPixels,
                    std::vector<FlatVertex>& out)
        : m_viewProjection(projection * view), m_out(out) {
        m_right = view.row(0).toVector3D();
        m_up = view.row(1).toVector3D();
        m_forward = -view.row(2).toVector3D();
        m_eye = view.inverted().map(QVector3D(0.0F, 0.0F, 0.0F));
        m_pixelsToWorld = 2.0F / (static_cast<float>(heightInPixels) * projection(1, 1));
    }

    // World size of one pixel at a point: depends on how far it is in a perspective view.
    float pixelSize(const QVector3D& p) const {
        const QVector4D clip = m_viewProjection * QVector4D(p, 1.0F);
        return clip.w() > 1e-6F ? clip.w() * m_pixelsToWorld : 0.0F;
    }

    void strip(const QVector3D& a, const QVector3D& b, float widthPixels, const QColor& color) {
        QVector3D direction = b - a;
        if (direction.lengthSquared() < 1e-12F) {
            return;
        }
        direction.normalize();
        QVector3D side = QVector3D::crossProduct(direction, m_forward);
        if (side.lengthSquared() < 1e-8F) {
            return; // the edge points at the viewer: nothing to see
        }
        side.normalize();
        const QVector3D sa = side * (0.5F * widthPixels * pixelSize(a));
        const QVector3D sb = side * (0.5F * widthPixels * pixelSize(b));
        push(a - sa, color);
        push(a + sa, color);
        push(b + sb, color);
        push(a - sa, color);
        push(b + sb, color);
        push(b - sb, color);
    }

    // A point moved towards the eye by so many pixels' worth, along its line of sight (it stays
    // where it is on screen): a dot on a face stays in front of it, even where the face is
    // slanted.
    QVector3D towardsViewer(const QVector3D& p, float pixels) const {
        QVector3D toEye = m_eye - p;
        if (toEye.lengthSquared() < 1e-12F) {
            return p;
        }
        return p + toEye.normalized() * (pixels * pixelSize(p));
    }

    // A disc facing the viewer, or a ring of it from `inner` to `outer` pixels.
    void ring(const QVector3D& center, float inner, float outer, const QColor& color) {
        constexpr int kSegments = 24;
        const float size = pixelSize(center);
        for (int i = 0; i < kSegments; ++i) {
            const float a0 = 6.2831853F * static_cast<float>(i) / kSegments;
            const float a1 = 6.2831853F * static_cast<float>(i + 1) / kSegments;
            const QVector3D d0 = (m_right * std::cos(a0) + m_up * std::sin(a0)) * size;
            const QVector3D d1 = (m_right * std::cos(a1) + m_up * std::sin(a1)) * size;
            if (inner <= 0.0F) {
                push(center, color);
                push(center + d0 * outer, color);
                push(center + d1 * outer, color);
                continue;
            }
            push(center + d0 * inner, color);
            push(center + d0 * outer, color);
            push(center + d1 * outer, color);
            push(center + d0 * inner, color);
            push(center + d1 * outer, color);
            push(center + d1 * inner, color);
        }
    }

    // A point so many pixels across and up from `center`, in the plane facing the viewer.
    QVector3D offset(const QVector3D& center, float across, float up) const {
        const float size = pixelSize(center);
        return center + m_right * (across * size) + m_up * (up * size);
    }

    // The way a world direction runs on screen, as a unit vector (across, up).
    QPointF screenWay(const QVector3D& direction) const {
        const QPointF way(QVector3D::dotProduct(direction, m_right),
                          QVector3D::dotProduct(direction, m_up));
        const double length = std::hypot(way.x(), way.y());
        return length > 1e-6 ? way / length : QPointF(0.0, 1.0);
    }

    void triangle(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QColor& color) {
        push(a, color);
        push(b, color);
        push(c, color);
    }

    void square(const QVector3D& center, float sidePixels, const QColor& color) {
        const float h = 0.5F * sidePixels * pixelSize(center);
        const QVector3D r = m_right * h;
        const QVector3D u = m_up * h;
        push(center - r - u, color);
        push(center + r - u, color);
        push(center + r + u, color);
        push(center - r - u, color);
        push(center + r + u, color);
        push(center - r + u, color);
    }

private:
    void push(const QVector3D& p, const QColor& color) {
        m_out.push_back(flatVertex(p.x(), p.y(), p.z(), color));
    }

    QMatrix4x4 m_viewProjection;
    QVector3D m_right, m_up, m_forward, m_eye;
    float m_pixelsToWorld = 0.0F;
    std::vector<FlatVertex>& m_out;
};

QVector3D toVector(const geo::Point3f& p) {
    return {p.x, p.y, p.z};
}

QColor withAlpha(QColor color, float alpha) {
    color.setAlphaF(alpha);
    return color;
}

QColor lighter(const QColor& color) {
    return QColor::fromRgbF(0.5F * color.redF() + 0.5F, 0.5F * color.greenF() + 0.5F,
                            0.5F * color.blueF() + 0.5F);
}

} // namespace

void SceneRenderer::prepareHighlights(QRhiResourceUpdateBatch* updates, const Frame& frame,
                                      const QSize& targetSize) {
    m_tintDraws.clear();
    m_ribbonVertexCount = 0;
    if (frame.highlights.empty() || !m_overlayPipeline) {
        return;
    }

    // Colours of the four tints: face under the pointer, chosen face, body under the pointer,
    // chosen body.
    const QColor styleColors[kOverlayStyles] = {
        withAlpha(lighter(frame.accent), 0.30F), withAlpha(frame.accent, 0.45F),
        withAlpha(lighter(frame.accent), 0.14F), withAlpha(frame.accent, 0.22F)};
    OverlayParams overlay{};
    copyMatrix(overlay.mvp, m_rhi->clipSpaceCorrMatrix() * frame.projection * frame.view);
    for (int style = 0; style < kOverlayStyles; ++style) {
        setColor(overlay.color, styleColors[style]);
        updates->updateDynamicBuffer(m_overlayUniforms.get(),
                                     static_cast<int>(m_overlayStride) * style,
                                     sizeof(OverlayParams), &overlay);
    }

    std::vector<FlatVertex> shapes;
    HighlightShapes builder(frame.view, frame.projection, std::max(1, targetSize.height()), shapes);
    for (const Highlight& highlight : frame.highlights) {
        const auto found = m_gpuBodies.find(highlight.body);
        if (found == m_gpuBodies.end() || !found->second.vertices) {
            continue;
        }
        const GpuBody& body = found->second;
        const geo::TriangleMesh* mesh = body.mesh.get();
        const QColor line = highlight.hover ? lighter(frame.accent) : frame.accent;
        const float lineWidth = highlight.hover ? 3.5F : 3.0F;
        switch (highlight.kind) {
        case core::SelectableKind::body:
            m_tintDraws.push_back({&body, highlight.hover ? 2 : 3, 0});
            if (mesh != nullptr) {
                for (const geo::EdgePolyline& edge : mesh->edges) {
                    for (std::size_t i = 0; i + 1 < edge.points.size(); ++i) {
                        builder.strip(toVector(edge.points[i]), toVector(edge.points[i + 1]), 2.0F,
                                      line);
                    }
                }
            }
            break;
        case core::SelectableKind::face:
            m_tintDraws.push_back({&body, highlight.hover ? 0 : 1, highlight.number});
            break;
        case core::SelectableKind::edge:
            if (mesh != nullptr) {
                for (const geo::EdgePolyline& edge : mesh->edges) {
                    if (edge.edgeId != highlight.number) {
                        continue;
                    }
                    for (std::size_t i = 0; i + 1 < edge.points.size(); ++i) {
                        builder.strip(toVector(edge.points[i]), toVector(edge.points[i + 1]),
                                      lineWidth, line);
                    }
                }
            }
            break;
        case core::SelectableKind::vertex:
            if (mesh != nullptr) {
                for (const geo::CornerPoint& corner : mesh->corners) {
                    if (corner.cornerId == highlight.number) {
                        builder.square(toVector(corner.position), highlight.hover ? 11.0F : 9.0F,
                                       line);
                    }
                }
            }
            break;
        }
    }

    m_ribbonVertexCount = shapes.size();
    if (shapes.empty()) {
        return;
    }
    if (!m_ribbonBuffer || m_ribbonCapacity < shapes.size()) {
        m_ribbonCapacity = std::max<std::size_t>(shapes.size() * 2, 4096);
        m_ribbonBuffer.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                             static_cast<quint32>(m_ribbonCapacity * sizeof(FlatVertex))));
        m_ribbonBuffer->create();
    }
    updates->updateDynamicBuffer(m_ribbonBuffer.get(), 0,
                                 static_cast<quint32>(shapes.size() * sizeof(FlatVertex)),
                                 shapes.data());
}

void HandleGraphics::append(const HandleGraphics& other) {
    lines.insert(lines.end(), other.lines.begin(), other.lines.end());
    dots.insert(dots.end(), other.dots.begin(), other.dots.end());
    arrows.insert(arrows.end(), other.arrows.begin(), other.arrows.end());
    heads.insert(heads.end(), other.heads.begin(), other.heads.end());
    quads.insert(quads.end(), other.quads.begin(), other.quads.end());
}

void SceneRenderer::prepareHandles(QRhiResourceUpdateBatch* updates, const Frame& frame,
                                   const QSize& targetSize) {
    m_handleVertexCount = 0;
    const HandleGraphics& handles = frame.handles;
    if (handles.empty() || !m_handlePipeline) {
        return;
    }
    std::vector<FlatVertex> shapes;
    // The builder works in window pixels: the image has `pixelScale` times as many.
    const float scale = std::max(handles.pixelScale, 0.1F);
    HighlightShapes builder(
        frame.view, frame.projection,
        std::max(1, static_cast<int>(static_cast<float>(targetSize.height()) / scale)), shapes);
    for (const HandleQuad& quad : handles.quads) {
        builder.triangle(quad.corners[0], quad.corners[1], quad.corners[2], quad.fill);
        builder.triangle(quad.corners[0], quad.corners[2], quad.corners[3], quad.fill);
        for (int i = 0; i < 4; ++i) {
            builder.strip(quad.corners[i], quad.corners[(i + 1) % 4], quad.lineWidth, quad.line);
        }
    }
    for (const HandleLine& line : handles.lines) {
        builder.strip(line.from, line.to, line.width, line.color);
    }
    for (const HandleHead& head : handles.heads) {
        // A triangle facing the viewer, from the tip back along the way it points on screen.
        const QPointF way = builder.screenWay(head.direction);
        const QVector3D back = builder.offset(head.tip, static_cast<float>(-head.length * way.x()),
                                              static_cast<float>(-head.length * way.y()));
        const QVector3D left = builder.offset(back, static_cast<float>(-head.halfWidth * way.y()),
                                              static_cast<float>(head.halfWidth * way.x()));
        const QVector3D right = builder.offset(back, static_cast<float>(head.halfWidth * way.y()),
                                               static_cast<float>(-head.halfWidth * way.x()));
        builder.triangle(head.tip, left, right, head.color);
    }
    for (const HandleArrow& arrow : handles.arrows) {
        // A chevron 13 to 19 pixels beyond the dot, a light one beneath a dark one.
        const QVector3D at = builder.towardsViewer(arrow.at, 10.0F);
        const QPointF way = builder.screenWay(arrow.direction);
        const QVector3D left =
            builder.offset(at, static_cast<float>(13.0 * way.x() - 5.0 * way.y()),
                           static_cast<float>(13.0 * way.y() + 5.0 * way.x()));
        const QVector3D tip = builder.offset(at, static_cast<float>(19.0 * way.x()),
                                             static_cast<float>(19.0 * way.y()));
        const QVector3D right =
            builder.offset(at, static_cast<float>(13.0 * way.x() + 5.0 * way.y()),
                           static_cast<float>(13.0 * way.y() - 5.0 * way.x()));
        for (const auto& [width, color] :
             {std::pair{4.5F, arrow.halo}, std::pair{2.0F, arrow.color}}) {
            builder.strip(left, tip, width, color);
            builder.strip(tip, right, width, color);
            builder.ring(tip, 0.0F, width / 2.0F, color); // a round joint
        }
    }
    for (const HandleDot& dot : handles.dots) {
        const QVector3D centre = builder.towardsViewer(dot.centre, 3.0F * dot.radius);
        builder.ring(centre, 0.0F, dot.radius, dot.fill);
        builder.ring(centre, dot.radius - dot.ringWidth, dot.radius, dot.ring);
        if (dot.rounding) {
            // A quarter circle round the lower left, bulging to the upper right: a round corner.
            constexpr int kSteps = 6;
            const float r = dot.radius * 0.95F;
            const float cx = -dot.radius * 0.45F;
            const float cy = -dot.radius * 0.45F;
            QVector3D last = builder.offset(centre, cx + r, cy);
            for (int i = 1; i <= kSteps; ++i) {
                const float a = 1.5707963F * static_cast<float>(i) / kSteps;
                const QVector3D next =
                    builder.offset(centre, cx + r * std::cos(a), cy + r * std::sin(a));
                builder.strip(last, next, 1.5F, dot.ring);
                last = next;
            }
        }
    }
    if (shapes.empty()) {
        return;
    }
    // The same triangles twice: faint for where a body is in front, full for the rest.
    const std::size_t half = shapes.size();
    shapes.reserve(2 * half);
    for (std::size_t i = 0; i < half; ++i) {
        FlatVertex faint = shapes[i];
        faint.color[3] *= std::clamp(handles.hiddenAlpha, 0.0F, 1.0F);
        shapes.push_back(faint);
    }
    m_handleVertexCount = shapes.size();
    if (!m_handleBuffer || m_handleCapacity < shapes.size()) {
        m_handleCapacity = std::max<std::size_t>(shapes.size() * 2, 2048);
        m_handleBuffer.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                             static_cast<quint32>(m_handleCapacity * sizeof(FlatVertex))));
        m_handleBuffer->create();
    }
    updates->updateDynamicBuffer(m_handleBuffer.get(), 0,
                                 static_cast<quint32>(shapes.size() * sizeof(FlatVertex)),
                                 shapes.data());
}

void SceneRenderer::drawHandles(QRhiCommandBuffer* cb) {
    if (m_handleVertexCount == 0 || !m_handleBuffer) {
        return;
    }
    const auto half = static_cast<quint32>(m_handleVertexCount / 2);
    const QRhiCommandBuffer::VertexInput input(m_handleBuffer.get(), 0);
    cb->setGraphicsPipeline(m_handleHiddenPipeline.get());
    cb->setShaderResources(m_srb.get());
    cb->setVertexInput(0, 1, &input);
    cb->draw(half, 1, half, 0); // the faint half
    cb->setGraphicsPipeline(m_handlePipeline.get());
    cb->setShaderResources(m_srb.get());
    cb->setVertexInput(0, 1, &input);
    cb->draw(half, 1, 0, 0);
}

void SceneRenderer::drawHighlights(QRhiCommandBuffer* cb) {
    if (!m_tintDraws.empty()) {
        cb->setGraphicsPipeline(m_overlayPipeline.get());
        for (const TintDraw& tint : m_tintDraws) {
            const GpuBody& body = *tint.body;
            const QRhiCommandBuffer::DynamicOffset offset{0, static_cast<quint32>(tint.style) *
                                                                 m_overlayStride};
            cb->setShaderResources(m_overlaySrb.get(), 1, &offset);
            const QRhiCommandBuffer::VertexInput input(body.vertices.get(), 0);
            cb->setVertexInput(0, 1, &input, body.indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
            if (tint.face == 0) {
                cb->drawIndexed(static_cast<quint32>(body.indexCount));
                continue;
            }
            auto run = std::lower_bound(body.faceRuns.begin(), body.faceRuns.end(), tint.face,
                                        [](const GpuBody::FaceRun& candidate, std::uint32_t face) {
                                            return candidate.face < face;
                                        });
            for (; run != body.faceRuns.end() && run->face == tint.face; ++run) {
                cb->drawIndexed(run->triangleCount * 3, 1, run->firstTriangle * 3, 0, 0);
            }
        }
    }
    if (m_ribbonVertexCount > 0 && m_ribbonBuffer) {
        const QRhiCommandBuffer::VertexInput input(m_ribbonBuffer.get(), 0);
        cb->setGraphicsPipeline(m_discPipeline.get());
        cb->setShaderResources(m_srb.get());
        cb->setVertexInput(0, 1, &input);
        cb->draw(static_cast<quint32>(m_ribbonVertexCount));
    }
}

} // namespace fission::render
