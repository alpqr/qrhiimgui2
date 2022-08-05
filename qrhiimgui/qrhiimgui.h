// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUI_H
#define QRHIIMGUI_H

#include <QtGui/private/qrhi_p.h>
#include <functional>

QT_BEGIN_NAMESPACE

class QRhiImguiPrivate;

// Owns graphics resources, so the QRhiImgui object itself should be owned by
// the appropriate render thread (if any) entity (e.g. a QSGNode).
class QRhiImgui
{
public:
    QRhiImgui();
    ~QRhiImgui();

    // main thread

    void setInputEventSource(QObject *src, QObject *filterOwner);
    void setEatInputEvents(bool enabled);

    // ...or the render thread but with the main thread blocked
    void updateInput(const QPointF &logicalOffset, float dpr);

    // render thread

    void initialize(QRhi *rhi);
    void releaseResources();

    typedef std::function<void()> FrameFunc;
    void setFrameFunc(FrameFunc f);
    FrameFunc frameFunc() const;

    void setDepthTest(bool enabled);

    bool prepareFrame(QRhiRenderTarget *rt, QRhiResourceUpdateBatch *dstResourceUpdates);
    void recordFrame(QRhiCommandBuffer *cb);

private:
    Q_DISABLE_COPY(QRhiImgui)
    QRhiImguiPrivate *d = nullptr;
};

QT_END_NAMESPACE

#endif
