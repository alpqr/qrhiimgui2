// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <rhi/qrhi.h>

struct Triangle
{
    void init(QRhi *rhi, QRhiCommandBuffer *cb, QRhiRenderPassDescriptor *rpDesc);
    void render(QRhiCommandBuffer *cb, QRhiRenderTarget *rt, const QColor &clearColor, float rotation, float opacity);

    QRhi *m_rhi = nullptr;

    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_ps;

    QSize m_lastSize;
    QMatrix4x4 m_viewProjection;
};

#endif
