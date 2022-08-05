// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUINODE_P_H
#define QRHIIMGUINODE_P_H

#include <QtQuick/qsgrendernode.h>
#include "qrhiimguiitem.h"
#include "qrhiimgui.h"

QT_BEGIN_NAMESPACE

class QRhiImguiNode : public QSGRenderNode
{
public:
    QRhiImguiNode(QQuickWindow *m_window);
    ~QRhiImguiNode();

    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;
    QRectF rect() const override;

    void initialize();
    void doReleaseResources();

    QQuickWindow *m_window;
    QRhi *m_rhi = nullptr;
    QSizeF m_itemSize;
    QRhiImgui m_imgui;
};

QT_END_NAMESPACE

#endif
