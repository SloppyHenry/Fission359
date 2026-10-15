// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/scene_renderer.h"

#include <QFile>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
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
};
static_assert(sizeof(Params) == 64 * 3 + 16 * 11, "Params must match the std140 block");

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
                                                   m_shadowTexture.get(), m_sampler.get())});
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

SceneRenderer::~SceneRenderer() = default;

std::unique_ptr<QRhiGraphicsPipeline>
SceneRenderer::makePipeline(const QShader& vertex, const QShader& fragment, bool colorLayoutOnly,
                            bool lines, bool blend, bool depth, QRhiRenderPassDescriptor* pass,
                            int samples) {
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
    pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    pipeline->setCullMode(QRhiGraphicsPipeline::None);
    if (!lines && depth) {
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
    pipeline->setShaderResourceBindings(m_srb.get());
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
    m_discPipeline =
        makePipeline(m_flatVertex, m_flatFragment, true, false, true, true, pass, sampleCount);
    m_linePipeline =
        makePipeline(m_flatVertex, m_flatFragment, true, true, true, true, pass, sampleCount);
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
            gpu.vertices = makeVertexBuffer(updates, vertices.data(),
                                            vertices.size() * sizeof(MeshVertex),
                                            QRhiBuffer::VertexBuffer);
            gpu.indices = makeVertexBuffer(updates, mesh.indices.data(),
                                           mesh.indices.size() * sizeof(std::uint32_t),
                                           QRhiBuffer::IndexBuffer);
            gpu.indexCount = mesh.indices.size();
            gpu.lines = makeVertexBuffer(updates, edgeLines.data(),
                                         edgeLines.size() * sizeof(FlatVertex),
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
        if (!body->vertices || !body->indices) {
            continue;
        }
        const QRhiCommandBuffer::VertexInput input(body->vertices.get(), 0);
        cb->setVertexInput(0, 1, &input, body->indices.get(), 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(static_cast<quint32>(body->indexCount));
    }
    cb->endPass();
}

void SceneRenderer::writeUniforms(QRhiResourceUpdateBatch* updates, QRhiBuffer* buffer,
                                  const QMatrix4x4& mvp, const Frame& frame) const {
    Params p{};
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

void SceneRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderTarget* target, const Frame& frame) {
    if (!m_bodyPipeline) {
        return;
    }
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    writeUniforms(updates, m_uniforms.get(),
                  m_rhi->clipSpaceCorrMatrix() * frame.projection * frame.view, frame);

    const QSize size = target->pixelSize();
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
        cb->setGraphicsPipeline(m_bodyPipeline.get());
        cb->setShaderResources(m_srb.get());
        for (const GpuBody* body : m_drawOrder) {
            if (!body->vertices || !body->indices) {
                continue;
            }
            const QRhiCommandBuffer::VertexInput input(body->vertices.get(), 0);
            cb->setVertexInput(0, 1, &input, body->indices.get(), 0,
                               QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(static_cast<quint32>(body->indexCount));
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
    cb->endPass();
}

} // namespace fission::render
