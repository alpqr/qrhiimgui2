// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <qglobal.h>

#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
#include <rhi/qrhi.h>
#else
#include <QtGui/private/qrhi_p.h>
#endif

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
