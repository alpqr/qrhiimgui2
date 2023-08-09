// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUI_H
#define QRHIIMGUI_H

#include <qglobal.h>

#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
#include <rhi/qrhi.h>
#else
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhinull_p.h>
#if QT_CONFIG(opengl)
#include <QtGui/private/qrhigles2_p.h>
#endif
#if QT_CONFIG(vulkan)
#include <QtGui/private/qrhivulkan_p.h>
#endif
#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#endif
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#include <QtGui/private/qrhimetal_p.h>
#endif
#endif

QT_BEGIN_NAMESPACE

class QEvent;

class QRhiImguiRenderer
{
public:
    ~QRhiImguiRenderer();

    struct CmdListBuffer {
        quint32 offset;
        QByteArray data;
    };

    struct DrawCmd {
        int cmdListBufferIdx;
        void *textureId;
        quint32 indexOffset;
        quint32 elemCount;
        QPointF itemPixelOffset;
        QVector4D clipRect;
    };

    struct StaticRenderData {
        QImage fontTextureData;
    };

    struct FrameRenderData {
        quint32 totalVbufSize = 0;
        quint32 totalIbufSize = 0;
        QVarLengthArray<CmdListBuffer, 4> vbuf;
        QVarLengthArray<CmdListBuffer, 4> ibuf;
        QVarLengthArray<DrawCmd, 4> draw;
        QSize outputPixelSize;
    };

    StaticRenderData sf;
    FrameRenderData f;

    void prepare(QRhi *rhi,
                 QRhiRenderTarget *rt,
                 QRhiCommandBuffer *cb,
                 const QMatrix4x4 &mvp,
                 float opacity = 1.0f,
                 float hdrWhiteLevelMultiplierOrZeroForSDRsRGB = 0.0f);
    void render();
    void releaseResources();

    enum CustomTextureOwnership {
        TakeCustomTextureOwnership,
        NoCustomTextureOwnership
    };
    void registerCustomTexture(void *id,
                               QRhiTexture *texture,
                               QRhiSampler::Filter filter,
                               CustomTextureOwnership ownership);

private:
    QRhi *m_rhi = nullptr;
    QRhiRenderTarget *m_rt = nullptr;
    QRhiCommandBuffer *m_cb = nullptr;

    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ibuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiGraphicsPipeline> m_ps;
    QVector<quint32> m_renderPassFormat;
    std::unique_ptr<QRhiSampler> m_linearSampler;
    std::unique_ptr<QRhiSampler> m_nearestSampler;

    struct Texture {
        QImage image;
        QRhiTexture *tex = nullptr;
        QRhiShaderResourceBindings *srb = nullptr;
        QRhiSampler::Filter filter = QRhiSampler::Linear;
        bool ownTex = true;
    };
    QHash<void *, Texture> m_textures;
};

class QRhiImgui
{
public:
    QRhiImgui();
    ~QRhiImgui();

    using FrameFunc = std::function<void()>;
    void nextFrame(const QSizeF &logicalOutputSize, float dpr, const QPointF &logicalOffset, FrameFunc frameFunc);
    void syncRenderer(QRhiImguiRenderer *renderer);
    bool processEvent(QEvent *e);

    void rebuildFontAtlas();
    void rebuildFontAtlasWithFont(const QString &filename);

private:
    QRhiImguiRenderer::StaticRenderData sf;
    QRhiImguiRenderer::FrameRenderData f;
    Qt::MouseButtons pressedMouseButtons;
};

QT_END_NAMESPACE

#endif
