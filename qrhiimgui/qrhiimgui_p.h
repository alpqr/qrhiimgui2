// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUI_P_H
#define QRHIIMGUI_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qrhiimgui.h"

#include "imgui.h"

QT_BEGIN_NAMESPACE

#define FIRSTSPECKEY (0x01000000)
#define LASTSPECKEY (0x01000017)
#define MAPSPECKEY(k) ((k) - FIRSTSPECKEY + 256)

class QRhiImGuiInputEventFilter : public QObject
{
public:
    QRhiImGuiInputEventFilter(QObject *parent)
        : QObject(parent)
    {
        memset(keyDown, 0, sizeof(keyDown));
    }

    bool eventFilter(QObject *watched, QEvent *event) override;

    bool eatEvents = false;
    QPointF mousePos;
    Qt::MouseButtons mouseButtonsDown = Qt::NoButton;
    float mouseWheel = 0;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    bool keyDown[256 + (LASTSPECKEY - FIRSTSPECKEY + 1)];
    QString keyText;
};

class QRhiImguiPrivate
{
public:
    QRhiImgui::FrameFunc frame = nullptr;
    QRhi *rhi = nullptr;

    struct Texture {
        QImage image;
        QRhiTexture *tex = nullptr;
        QRhiShaderResourceBindings *srb = nullptr;
    };
    QVector<Texture> textures;

    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ibuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;
    QRhiSampler *sampler = nullptr;
    QVector<QRhiResource *> releasePool;
    QRhiRenderTarget *rt;
    QSize lastOutputSize;
    QVarLengthArray<quint32, 4> vbufOffsets;
    QVarLengthArray<quint32, 4> ibufOffsets;

    QRhiImGuiInputEventFilter *inputEventFilter = nullptr;
    QObject *inputEventSource = nullptr;
    bool inputInitialized = false;

    bool depthTest = true;
};

QT_END_NAMESPACE

#endif
