// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimguinode_p.h"
#include <QtCore/qfile.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/private/qsgrendernode_p.h>

QT_BEGIN_NAMESPACE

static QShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());

    return QShader();
}

QRhiImguiNode::QRhiImguiNode(QQuickWindow *window)
    : m_window(window)
{
}

QRhiImguiNode::~QRhiImguiNode()
{
    doReleaseResources();
}

void QRhiImguiNode::releaseResources()
{
    doReleaseResources();
}

void QRhiImguiNode::doReleaseResources()
{
    for (Texture &t : m_textures) {
        delete t.tex;
        delete t.srb;
    }
    m_textures.clear();

    m_vbuf.reset();
    m_ibuf.reset();
    m_ubuf.reset();
    m_ps.reset();
    m_sampler.reset();

    m_rhi = nullptr;
}

void QRhiImguiNode::prepare()
{
    if (!m_rhi) {
        QSGRendererInterface *rif = m_window->rendererInterface();
        m_rhi = static_cast<QRhi *>(rif->getResource(m_window, QSGRendererInterface::RhiResource));
        if (!m_rhi) {
            qWarning("No QRhi found for window %p, QRhiImguiItem will not be functional", m_window);
            return;
        }
    }

    if (!m_rhi || f.draw.isEmpty())
        return;

    QSGRenderNodePrivate *d = QSGRenderNodePrivate::get(this);
    m_rt = d->m_rt.rt;
    m_cb = d->m_rt.cb;

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();

    if (!m_vbuf) {
        m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, f.totalVbufSize));
        m_vbuf->setName(QByteArrayLiteral("imgui vertex buffer"));
        if (!m_vbuf->create())
            return;
    } else {
        if (f.totalVbufSize > m_vbuf->size()) {
            m_vbuf->setSize(f.totalVbufSize);
            if (!m_vbuf->create())
                return;
        }
    }
    if (!m_ibuf) {
        m_ibuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, f.totalIbufSize));
        m_ibuf->setName(QByteArrayLiteral("imgui index buffer"));
        if (!m_ibuf->create())
            return;
    } else {
        if (f.totalIbufSize > m_ibuf->size()) {
            m_ibuf->setSize(f.totalIbufSize);
            if (!m_ibuf->create())
                return;
        }
    }

    if (!m_ubuf) {
        m_ubuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 4));
        m_ubuf->setName(QByteArrayLiteral("imgui uniform buffer"));
        if (!m_ubuf->create())
            return;
        m_lastOutputPixelSize = QSize();
        m_lastOpacity = inheritedOpacity();
        u->updateDynamicBuffer(m_ubuf.get(), 64, 4, &m_lastOpacity);
    }

    for (const CmdListBuffer &b : f.vbuf)
        u->updateDynamicBuffer(m_vbuf.get(), b.offset, b.data.size(), b.data.constData());

    for (const CmdListBuffer &b : f.ibuf)
        u->updateDynamicBuffer(m_ibuf.get(), b.offset, b.data.size(), b.data.constData());

    if (m_lastOutputPixelSize != f.outputPixelSize) {
        m_lastOutputPixelSize = f.outputPixelSize;
        const QMatrix4x4 mvp = *projectionMatrix() * *matrix();
        u->updateDynamicBuffer(m_ubuf.get(), 0, 64, mvp.constData());
    }

    const float opacity = inheritedOpacity();
    if (m_lastOpacity != opacity) {
        m_lastOpacity = opacity;
        u->updateDynamicBuffer(m_ubuf.get(), 64, 4, &m_lastOpacity);
    }

    if (!m_sampler) {
        m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                          QRhiSampler::Repeat, QRhiSampler::Repeat));
        m_sampler->setName(QByteArrayLiteral("imgui sampler"));
        if (!m_sampler->create())
            return;
    }

    if (m_textures.isEmpty()) {
        Texture fontTex;
        fontTex.image = sf.fontTextureData;
        m_textures.append(fontTex);
    }

    for (int i = 0; i < m_textures.count(); ++i) {
        Texture &t(m_textures[i]);
        if (!t.tex) {
            t.tex = m_rhi->newTexture(QRhiTexture::RGBA8, t.image.size());
            t.tex->setName(QByteArrayLiteral("imgui texture ") + QByteArray::number(i));
            if (!t.tex->create())
                return;
            u->uploadTexture(t.tex, t.image);
        }
        if (!t.srb) {
            t.srb = m_rhi->newShaderResourceBindings();
            t.srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_ubuf.get()),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, t.tex, m_sampler.get())
            });
            if (!t.srb->create())
                return;
        }
    }

    // If layer.enabled is toggled on the item or an ancestor, the render
    // target is then suddenly different and may not be compatible.
    if (m_ps && m_rt->renderPassDescriptor()->serializedFormat() != m_renderPassFormat)
        m_ps.reset();

    if (!m_ps) {
        m_ps.reset(m_rhi->newGraphicsPipeline());
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        // Premultiplied alpha (matches imgui.frag). Would not be needed if we
        // only cared about outputting to the window (the common case), but
        // once going through a texture (Item layer, ShaderEffect) which is
        // then sampled by Quick, the result wouldn't be correct otherwise.
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        m_ps->setTargetBlends({ blend });
        m_ps->setCullMode(QRhiGraphicsPipeline::None);
        m_ps->setDepthTest(true);
        m_ps->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        m_ps->setDepthWrite(false);
        m_ps->setFlags(QRhiGraphicsPipeline::UsesScissor);

        QShader vs = getShader(QLatin1String(":/imgui.vert.qsb"));
        Q_ASSERT(vs.isValid());
        QShader fs = getShader(QLatin1String(":/imgui.frag.qsb"));
        Q_ASSERT(fs.isValid());
        m_ps->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fs }
        });

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 4 * sizeof(float) + sizeof(quint32) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
            { 0, 2, QRhiVertexInputAttribute::UNormByte4, 4 * sizeof(float) }
        });

        m_ps->setVertexInputLayout(inputLayout);
        m_ps->setShaderResourceBindings(m_textures[0].srb);
        m_ps->setRenderPassDescriptor(m_rt->renderPassDescriptor());
        m_renderPassFormat = m_rt->renderPassDescriptor()->serializedFormat();

        if (!m_ps->create())
            return;
    }

    m_cb->resourceUpdate(u);
}

void QRhiImguiNode::render(const RenderState *)
{
    if (!m_rhi || f.draw.isEmpty())
        return;

    m_cb->setGraphicsPipeline(m_ps.get());

    const QSize viewportSize = m_rt->pixelSize();
    bool needsViewport = true;

    for (const DrawCmd &c : f.draw) {
        QRhiCommandBuffer::VertexInput vbufBinding(m_vbuf.get(), f.vbuf[c.cmdListBufferIdx].offset);
        if (needsViewport) {
            needsViewport = false;
            m_cb->setViewport({ 0, 0, float(viewportSize.width()), float(viewportSize.height()) });
        }
        QPoint scissorPos = QPointF(c.clipRect.x(), viewportSize.height() - c.clipRect.w()).toPoint();
        QSize scissorSize = QSizeF(c.clipRect.z() - c.clipRect.x(), c.clipRect.w() - c.clipRect.y()).toSize();
        scissorPos.setX(qMax(0, scissorPos.x()));
        scissorPos.setY(qMax(0, scissorPos.y()));
        scissorSize.setWidth(qMin(viewportSize.width(), scissorSize.width()));
        scissorSize.setHeight(qMin(viewportSize.height(), scissorSize.height()));
        m_cb->setScissor({ scissorPos.x(), scissorPos.y(), scissorSize.width(), scissorSize.height() });
        m_cb->setShaderResources(m_textures[c.textureIndex].srb);
        m_cb->setVertexInput(0, 1, &vbufBinding, m_ibuf.get(), c.indexOffset, QRhiCommandBuffer::IndexUInt32);
        m_cb->drawIndexed(c.elemCount);
    }
}

QSGRenderNode::StateFlags QRhiImguiNode::changedStates() const
{
    return DepthState | ScissorState | ColorState | BlendState | CullState | ViewportState;
}

QSGRenderNode::RenderingFlags QRhiImguiNode::flags() const
{
    // Don't want rhi->begin/endExternal() to be called by Quick since we work
    // with QRhi.
    QSGRenderNode::RenderingFlags result = NoExternalRendering;

    // If we take the projectionMatrix() adjustments into account then can
    // report DepthAwareRendering and so QQ will not disable the opaque pass.
    // (otherwise a visible QRhiImguiNode forces all batches to be part of the
    // back-to-front no-depth-write pass -> less optimal)
    result |= DepthAwareRendering;

    return result;
}

QT_END_NAMESPACE
