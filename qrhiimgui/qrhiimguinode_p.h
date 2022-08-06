// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUINODE_P_H
#define QRHIIMGUINODE_P_H

#include <QtQuick/qsgrendernode.h>
#include <QtGui/private/qrhi_p.h>

QT_BEGIN_NAMESPACE

class QQuickWindow;

class QRhiImguiNode : public QSGRenderNode
{
public:
    QRhiImguiNode(QQuickWindow *m_window);
    ~QRhiImguiNode();

    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;

    void doReleaseResources();

    struct CmdListBuffer {
        quint32 offset;
        QByteArray data;
    };

    struct DrawCmd {
        int cmdListBufferIdx;
        quint32 indexOffset;
        int textureIndex;
        QPoint scissorBottomLeft;
        QSize scissorSize;
        quint32 elemCount;
    };

    struct FrameRenderData {
        QMatrix4x4 mvp;
        QImage fontTextureData;
        quint32 totalVbufSize = 0;
        quint32 totalIbufSize = 0;
        QVector<CmdListBuffer> vbuf;
        QVector<CmdListBuffer> ibuf;
        QVector<DrawCmd> draw;
    };

    QQuickWindow *m_window;
    QRhi *m_rhi = nullptr;
    QRhiRenderTarget *m_rt = nullptr;
    QRhiCommandBuffer *m_cb = nullptr;
    QSize m_lastOutputSize;
    FrameRenderData f;

    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ibuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiGraphicsPipeline> m_ps;
    std::unique_ptr<QRhiSampler> m_sampler;

    struct Texture {
        QImage image;
        QRhiTexture *tex = nullptr;
        QRhiShaderResourceBindings *srb = nullptr;
    };
    QVector<Texture> m_textures;
};

QT_END_NAMESPACE

#endif
