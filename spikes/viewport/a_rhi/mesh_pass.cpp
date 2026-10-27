// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mesh_pass.h"

#include <QFile>

#include <algorithm>

namespace fission::spike {

namespace {

constexpr int kPickSize = 4;       // the pick target is 4 x 4 pixels, all showing one screen pixel
constexpr int kUniformBytes = 208; // mvp, light matrix, ids, color, eye, light, flags
constexpr int kShadowSize = 2048;

QShader loadShader(const QString& name) {
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Shader missing: %s", qPrintable(name));
    }
    return QShader::fromSerialized(file.readAll());
}

} // namespace

MeshPass::MeshPass(QRhi* rhi) : m_rhi(rhi) {
    const auto stages = QRhiShaderResourceBinding::VertexStage |
                        QRhiShaderResourceBinding::FragmentStage;
    m_uniforms.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes));
    m_uniforms->create();
    m_pickUniforms.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes));
    m_pickUniforms->create();
    m_shadowUniforms.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes));
    m_shadowUniforms->create();

    // shadow map: depth only, rendered once per scene
    m_shadowTexture.reset(m_rhi->newTexture(QRhiTexture::D32F, QSize(kShadowSize, kShadowSize), 1,
                                            QRhiTexture::RenderTarget));
    m_shadowTexture->create();
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                      QRhiSampler::None, QRhiSampler::ClampToEdge,
                                      QRhiSampler::ClampToEdge));
    m_sampler->create();
    QRhiTextureRenderTargetDescription shadowDescription;
    shadowDescription.setDepthTexture(m_shadowTexture.get());
    m_shadowTarget.reset(m_rhi->newTextureRenderTarget(shadowDescription));
    m_shadowPass.reset(m_shadowTarget->newCompatibleRenderPassDescriptor());
    m_shadowTarget->setRenderPassDescriptor(m_shadowPass.get());
    m_shadowTarget->create();

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, stages, m_uniforms.get()),
                        QRhiShaderResourceBinding::sampledTexture(
                            1, QRhiShaderResourceBinding::FragmentStage, m_shadowTexture.get(),
                            m_sampler.get())});
    m_srb->create();
    m_pickSrb.reset(m_rhi->newShaderResourceBindings());
    m_pickSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_pickUniforms.get())});
    m_pickSrb->create();
    m_shadowSrb.reset(m_rhi->newShaderResourceBindings());
    m_shadowSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, stages, m_shadowUniforms.get())});
    m_shadowSrb->create();

    m_pickTexture.reset(m_rhi->newTexture(
        QRhiTexture::RGBA8, QSize(kPickSize, kPickSize), 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    m_pickTexture->create();
    m_pickDepth.reset(
        m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, QSize(kPickSize, kPickSize), 1));
    m_pickDepth->create();
    QRhiTextureRenderTargetDescription description{QRhiColorAttachment(m_pickTexture.get())};
    description.setDepthStencilBuffer(m_pickDepth.get());
    m_pickTarget.reset(m_rhi->newTextureRenderTarget(description));
    m_pickPass.reset(m_pickTarget->newCompatibleRenderPassDescriptor());
    m_pickTarget->setRenderPassDescriptor(m_pickPass.get());
    m_pickTarget->create();

    m_vertexShader = loadShader(QStringLiteral(":/shaders/mesh.vert.qsb"));
    m_meshFragment = loadShader(QStringLiteral(":/shaders/mesh.frag.qsb"));
    m_pickFragment = loadShader(QStringLiteral(":/shaders/pick.frag.qsb"));
    m_lineVertex = loadShader(QStringLiteral(":/shaders/line.vert.qsb"));
    m_lineFragment = loadShader(QStringLiteral(":/shaders/line.frag.qsb"));
    m_shadowVertex = loadShader(QStringLiteral(":/shaders/shadow.vert.qsb"));
    m_shadowFragment = loadShader(QStringLiteral(":/shaders/shadow.frag.qsb"));

    m_pickPipeline = makePipeline(m_vertexShader, m_pickFragment, true, false, m_pickPass.get(), 1,
                                  m_pickSrb.get());
    // depth-only pipeline: position from the mesh layout, no color attachment
    m_shadowPipeline.reset(m_rhi->newGraphicsPipeline());
    m_shadowPipeline->setShaderStages({{QRhiShaderStage::Vertex, m_shadowVertex},
                                       {QRhiShaderStage::Fragment, m_shadowFragment}});
    QRhiVertexInputLayout shadowLayout;
    shadowLayout.setBindings({{TriMesh::kFloatsPerVertex * sizeof(float)}});
    shadowLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});
    m_shadowPipeline->setVertexInputLayout(shadowLayout);
    m_shadowPipeline->setDepthTest(true);
    m_shadowPipeline->setDepthWrite(true);
    m_shadowPipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_shadowPipeline->setDepthBias(4);
    m_shadowPipeline->setSlopeScaledDepthBias(2.0F);
    m_shadowPipeline->setTargetBlends({}); // no color attachment
    m_shadowPipeline->setShaderResourceBindings(m_shadowSrb.get());
    m_shadowPipeline->setRenderPassDescriptor(m_shadowPass.get());
    m_shadowPipeline->create();
}

void MeshPass::createPipelines(QRhiRenderPassDescriptor* pass, int sampleCount) {
    m_pipeline = makePipeline(m_vertexShader, m_meshFragment, true, false, pass, sampleCount,
                              m_srb.get());
    m_linePipeline = makePipeline(m_lineVertex, m_lineFragment, false, true, pass, sampleCount,
                                  m_srb.get());
}

std::unique_ptr<QRhiGraphicsPipeline> MeshPass::makePipeline(
    const QShader& vertex, const QShader& fragment, bool meshLayout, bool lines,
    QRhiRenderPassDescriptor* pass, int samples, QRhiShaderResourceBindings* bindings) {
    std::unique_ptr<QRhiGraphicsPipeline> pipeline(m_rhi->newGraphicsPipeline());
    pipeline->setShaderStages(
        {{QRhiShaderStage::Vertex, vertex}, {QRhiShaderStage::Fragment, fragment}});
    QRhiVertexInputLayout layout;
    if (meshLayout) {
        layout.setBindings({{TriMesh::kFloatsPerVertex * sizeof(float)}});
        layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                              {0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)},
                              {0, 2, QRhiVertexInputAttribute::Float, 6 * sizeof(float)}});
    } else {
        layout.setBindings({{3 * sizeof(float)}});
        layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});
    }
    pipeline->setVertexInputLayout(layout);
    pipeline->setTopology(lines ? QRhiGraphicsPipeline::Lines : QRhiGraphicsPipeline::Triangles);
    pipeline->setDepthTest(true);
    pipeline->setDepthWrite(true);
    pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    pipeline->setCullMode(QRhiGraphicsPipeline::None);
    if (!lines) {
        // Push surfaces back a little so that the edge lines win the depth test.
        pipeline->setDepthBias(2);
        pipeline->setSlopeScaledDepthBias(1.0F);
    }
    pipeline->setSampleCount(samples);
    pipeline->setShaderResourceBindings(bindings);
    pipeline->setRenderPassDescriptor(pass);
    pipeline->create();
    return pipeline;
}

void MeshPass::upload(QRhiCommandBuffer* cb, const Scene& scene) {
    const TriMesh& mesh = scene.mesh;
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                          quint32(mesh.vertices.size() * sizeof(float))));
    m_vertexBuffer->create();
    updates->uploadStaticBuffer(m_vertexBuffer.get(), mesh.vertices.data());
    m_indexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                                         quint32(mesh.indices.size() * sizeof(quint32))));
    m_indexBuffer->create();
    updates->uploadStaticBuffer(m_indexBuffer.get(), mesh.indices.data());
    m_indexCount = mesh.indices.size();
    m_edgeBuffer.reset();
    m_edgeVertexCount = 0;
    if (!mesh.edgePoints.empty()) {
        m_edgeBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                            quint32(mesh.edgePoints.size() * sizeof(float))));
        m_edgeBuffer->create();
        updates->uploadStaticBuffer(m_edgeBuffer.get(), mesh.edgePoints.data());
        m_edgeVertexCount = mesh.edgePoints.size() / 3;
    }

    // print bed (face id -1 marks it for the shader), larger than the scene
    const auto& lo = mesh.boundsMin;
    const auto& hi = mesh.boundsMax;
    const float cx = 0.5F * (lo[0] + hi[0]);
    const float cy = 0.5F * (lo[1] + hi[1]);
    const float half = std::max(128.0F, 0.75F * std::max(hi[0] - lo[0], hi[1] - lo[1]));
    const float z = lo[2] - 0.02F;
    const float bed[28] = {cx - half, cy - half, z, 0, 0, 1, -1.0F, cx + half, cy - half, z, 0, 0, 1,
                           -1.0F,     cx + half, cy + half, z, 0, 0, 1, -1.0F, cx - half, cy + half,
                           z,         0,         0,         1, -1.0F};
    const quint32 bedIndices[6] = {0, 1, 2, 0, 2, 3};
    m_bedVertexBuffer.reset(
        m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof bed));
    m_bedVertexBuffer->create();
    updates->uploadStaticBuffer(m_bedVertexBuffer.get(), bed);
    m_bedIndexBuffer.reset(
        m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, sizeof bedIndices));
    m_bedIndexBuffer->create();
    updates->uploadStaticBuffer(m_bedIndexBuffer.get(), bedIndices);

    // directional light from above-left: orthographic shadow frustum around the scene
    const QVector3D center(cx, cy, 0.5F * (lo[2] + hi[2]));
    const float radius =
        0.5F * QVector3D(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]).length() * 1.05F;
    m_lightDirection = QVector3D(-0.4F, -0.5F, 0.8F).normalized();
    QMatrix4x4 lightView;
    lightView.lookAt(center + m_lightDirection * radius * 2.0F, center, QVector3D(0, 0, 1));
    QMatrix4x4 lightProjection;
    lightProjection.ortho(-radius, radius, -radius, radius, radius * 0.2F, radius * 4.0F);
    m_lightViewProjection = lightProjection * lightView;

    cb->resourceUpdate(updates);
    m_uploaded = &scene;
    renderShadow(cb);
}

void MeshPass::renderShadow(QRhiCommandBuffer* cb) {
    if (!m_uploaded) {
        return;
    }
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    Frame none;
    writeUniforms(updates, m_shadowUniforms.get(),
                  m_rhi->clipSpaceCorrMatrix() * m_lightViewProjection, none);
    const QRhiCommandBuffer::VertexInput meshInput(m_vertexBuffer.get(), 0);
    cb->beginPass(m_shadowTarget.get(), QColor(0, 0, 0, 255), {1.0F, 0}, updates);
    cb->setGraphicsPipeline(m_shadowPipeline.get());
    cb->setViewport({0, 0, float(kShadowSize), float(kShadowSize)});
    cb->setShaderResources(m_shadowSrb.get());
    cb->setVertexInput(0, 1, &meshInput, m_indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(quint32(m_indexCount));
    cb->endPass();
}

void MeshPass::writeUniforms(QRhiResourceUpdateBatch* updates, QRhiBuffer* buffer,
                             const QMatrix4x4& mvp, const Frame& frame) {
    updates->updateDynamicBuffer(buffer, 0, 64, mvp.constData());
    updates->updateDynamicBuffer(buffer, 64, 64, m_lightViewProjection.constData());
    const float ids[4] = {float(frame.hoverFace), float(frame.selectedFace), 0, 0};
    updates->updateDynamicBuffer(buffer, 128, 16, ids);
    const float color[4] = {0.62F, 0.66F, 0.72F, 1.0F};
    updates->updateDynamicBuffer(buffer, 144, 16, color);
    const QVector3D eye = frame.view.inverted().map(QVector3D(0, 0, 0));
    const float eyeData[4] = {eye.x(), eye.y(), eye.z(), 1.0F};
    updates->updateDynamicBuffer(buffer, 160, 16, eyeData);
    const float lightData[4] = {m_lightDirection.x(), m_lightDirection.y(), m_lightDirection.z(),
                                0.0F};
    updates->updateDynamicBuffer(buffer, 176, 16, lightData);
    const float flags[4] = {m_rhi->isYUpInFramebuffer() ? 0.0F : 1.0F,
                            1.0F / float(kShadowSize), 0, 0};
    updates->updateDynamicBuffer(buffer, 192, 16, flags);
}

void MeshPass::draw(QRhiCommandBuffer* cb, QRhiRenderTarget* target, const Frame& frame,
                    const QColor& clearColor) {
    if (!m_uploaded || !m_pipeline) {
        return;
    }
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    const QMatrix4x4 correction = m_rhi->clipSpaceCorrMatrix();
    writeUniforms(updates, m_uniforms.get(), correction * frame.projection * frame.view, frame);

    const QSize size = target->pixelSize();
    const QRhiCommandBuffer::VertexInput meshInput(m_vertexBuffer.get(), 0);
    cb->beginPass(target, clearColor, {1.0F, 0}, updates);
    cb->setViewport({0, 0, float(size.width()), float(size.height())});
    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setShaderResources(m_srb.get());
    cb->setVertexInput(0, 1, &meshInput, m_indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(quint32(m_indexCount));
    if (m_bedVertexBuffer) {
        const QRhiCommandBuffer::VertexInput bedInput(m_bedVertexBuffer.get(), 0);
        cb->setVertexInput(0, 1, &bedInput, m_bedIndexBuffer.get(), 0,
                           QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(6);
    }
    if (m_edgeBuffer) {
        const QRhiCommandBuffer::VertexInput lineInput(m_edgeBuffer.get(), 0);
        cb->setGraphicsPipeline(m_linePipeline.get());
        cb->setShaderResources(m_srb.get());
        cb->setVertexInput(0, 1, &lineInput);
        cb->draw(quint32(m_edgeVertexCount));
    }
    cb->endPass();
}

bool MeshPass::pick(QRhiCommandBuffer* cb, const QSize& targetSize, const QPoint& pixel,
                    const Frame& frame, std::function<void(int)> done) {
    if (m_pickInFlight || !m_uploaded) {
        return false;
    }
    // Crop the projection to the pixel under the cursor: that one pixel fills the whole
    // 4 x 4 pick target.
    const float width = float(targetSize.width());
    const float height = float(targetSize.height());
    const float nx = 2.0F * (float(pixel.x()) + 0.5F) / width - 1.0F;
    const float ny = 1.0F - 2.0F * (float(pixel.y()) + 0.5F) / height;
    QMatrix4x4 crop;
    crop(0, 0) = width;
    crop(1, 1) = height;
    crop(0, 3) = -nx * width;
    crop(1, 3) = -ny * height;

    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    writeUniforms(updates, m_pickUniforms.get(),
                  m_rhi->clipSpaceCorrMatrix() * crop * frame.projection * frame.view, frame);

    const QRhiCommandBuffer::VertexInput meshInput(m_vertexBuffer.get(), 0);
    cb->beginPass(m_pickTarget.get(), QColor(0, 0, 0, 255), {1.0F, 0}, updates);
    cb->setGraphicsPipeline(m_pickPipeline.get());
    cb->setViewport({0, 0, float(kPickSize), float(kPickSize)});
    cb->setShaderResources(m_pickSrb.get());
    cb->setVertexInput(0, 1, &meshInput, m_indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(quint32(m_indexCount));

    m_pickDone = std::move(done);
    m_pickResult = {};
    m_pickResult.completed = [this] {
        int faceId = 0;
        if (m_pickResult.data.size() >= 4) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(m_pickResult.data.constData());
            faceId = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
        }
        m_pickInFlight = false;
        if (m_pickDone) {
            m_pickDone(faceId);
        }
    };
    QRhiResourceUpdateBatch* readback = m_rhi->nextResourceUpdateBatch();
    readback->readBackTexture(QRhiReadbackDescription(m_pickTexture.get()), &m_pickResult);
    m_pickInFlight = true;
    cb->endPass(readback);
    return true;
}

} // namespace fission::spike
