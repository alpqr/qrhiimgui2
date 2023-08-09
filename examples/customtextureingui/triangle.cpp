// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "triangle.h"
#include <QFile>

static float vertexData[] = {
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,
};

static QShader getShader(const QString &name)
{
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

void Triangle::init(QRhi *rhi, QRhiCommandBuffer *cb, QRhiRenderPassDescriptor *rpDesc)
{
    m_rhi = rhi;

    m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData)));
    m_vbuf->create();

    m_ubuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68));
    m_ubuf->create();

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                                                 m_ubuf.get())
    });
    m_srb->create();

    m_ps.reset(m_rhi->newGraphicsPipeline());

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    m_ps->setTargetBlends({ premulAlphaBlend });

    m_ps->setShaderStages({
        { QRhiShaderStage::Vertex, getShader(QLatin1String(":/shaders/color.vert.qsb")) },
        { QRhiShaderStage::Fragment, getShader(QLatin1String(":/shaders/color.frag.qsb")) }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 5 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 2 * sizeof(float) }
    });

    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb.get());
    m_ps->setRenderPassDescriptor(rpDesc);

    m_ps->create();

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    u->uploadStaticBuffer(m_vbuf.get(), vertexData);
    cb->resourceUpdate(u);
}

void Triangle::render(QRhiCommandBuffer *cb, QRhiRenderTarget *rt, const QColor &clearColor, float rotation, float opacity)
{
    const QSize outputSizeInPixels = rt->pixelSize();
    if (m_lastSize != outputSizeInPixels) {
        m_lastSize = outputSizeInPixels;
        m_viewProjection = m_rhi->clipSpaceCorrMatrix();
        m_viewProjection.perspective(45.0f, outputSizeInPixels.width() / (float) outputSizeInPixels.height(), 0.01f, 1000.0f);
        m_viewProjection.translate(0, 0, -4);
    }

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    QMatrix4x4 mvp = m_viewProjection;
    mvp.rotate(rotation, 0, 1, 0);
    u->updateDynamicBuffer(m_ubuf.get(), 0, 64, mvp.constData());
    u->updateDynamicBuffer(m_ubuf.get(), 64, 4, &opacity);

    cb->beginPass(rt, clearColor, { 1.0f, 0 }, u);

    cb->setGraphicsPipeline(m_ps.get());
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setShaderResources();

    const QRhiCommandBuffer::VertexInput vbufBinding(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(3);

    cb->endPass();
}
