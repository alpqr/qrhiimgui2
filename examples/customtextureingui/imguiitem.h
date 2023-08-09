// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef IMGUIITEM_H
#define IMGUIITEM_H

#include "qrhiimguiitem.h"

class QRhiTexture;
class QRhiTextureRenderTarget;
class QRhiRenderPassDescriptor;
struct Triangle;

class CustomRenderer : public QRhiImguiItemCustomRenderer
{
public:
    CustomRenderer(QQuickWindow *w) : window(w) { }
    ~CustomRenderer();
    void sync(QRhiImguiRenderer *renderer) override;
    void render() override;

    QQuickWindow *window;

    struct CustomContent {
        QRhiTexture *texture = nullptr;
        float dpr = 0;
    };

    CustomContent swPainted;
    CustomContent rhiRendered;

    bool swPaintedDirty = false;
    Triangle *triangleRenderer = nullptr;
    QRhiTextureRenderTarget *triRt = nullptr;
    QRhiRenderPassDescriptor *triRpDesc = nullptr;
    float triRotation = 0.0f;
};

class ImguiItem : public QRhiImguiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Imgui)

public:
    ImguiItem();
    void frame() override;
    QRhiImguiItemCustomRenderer *createCustomRenderer() override;

    CustomRenderer *cr = nullptr;
    bool demoWindowOpen = true;
};

#endif
