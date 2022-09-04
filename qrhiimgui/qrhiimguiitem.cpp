// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimguiitem.h"
#include "qrhiimgui_p.h"
#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/private/qsgrendernode_p.h>

#include "imgui.h"

QT_BEGIN_NAMESPACE

// QSGRenderNode::projectionMatrix() is only in 6.5+, earlier versions only
// have it in the RenderState and that's not available in prepare()
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#define WELL_BEHAVING_DEPTH 1
#endif

struct QRhiImguiNode : public QSGRenderNode
{
    QRhiImguiNode(QQuickWindow *m_window);
    ~QRhiImguiNode();

    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;

    QQuickWindow *window;
    QRhiImguiRenderer *renderer;
};

QRhiImguiNode::QRhiImguiNode(QQuickWindow *window)
    : window(window),
      renderer(new QRhiImguiRenderer)
{
}

QRhiImguiNode::~QRhiImguiNode()
{
    delete renderer;
}

void QRhiImguiNode::releaseResources()
{
    renderer->releaseResources();
}

void QRhiImguiNode::prepare()
{
    QSGRendererInterface *rif = window->rendererInterface();
    QRhi *rhi = static_cast<QRhi *>(rif->getResource(window, QSGRendererInterface::RhiResource));
    if (!rhi) {
        qWarning("QRhiImguiNode: No QRhi found for window %p", window);
        return;
    }

    QSGRenderNodePrivate *d = QSGRenderNodePrivate::get(this);
    QRhiRenderTarget *rt = d->m_rt.rt;
    QRhiCommandBuffer *cb = d->m_rt.cb;

#if WELL_BEHAVING_DEPTH
    const QMatrix4x4 mvp = *projectionMatrix() * *matrix();
#else
    QMatrix4x4 mvp = rhi->clipSpaceCorrMatrix();
    const QSize outputSize = rt->pixelSize();
    const float dpr = rt->devicePixelRatio();
    mvp.ortho(0, outputSize.width() / dpr, outputSize.height() / dpr, 0, 1, -1);
    mvp *= *matrix();
#endif

    const float opacity = inheritedOpacity();

    renderer->prepare(rhi, rt, cb, mvp, opacity);
}

void QRhiImguiNode::render(const RenderState *)
{
    renderer->render();
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

#if WELL_BEHAVING_DEPTH
    // If we take the projectionMatrix() adjustments into account then can
    // report DepthAwareRendering and so QQ will not disable the opaque pass.
    // (otherwise a visible QRhiImguiNode forces all batches to be part of the
    // back-to-front no-depth-write pass -> less optimal)
    result |= DepthAwareRendering;
#endif

    return result;
}

struct QRhiImguiItemPrivate
{
    QRhiImguiItem *q;
    QQuickWindow *window = nullptr;
    QMetaObject::Connection windowConn;
    QRhiImgui gui;
    bool showDemoWindow = true;

    QRhiImguiItemPrivate(QRhiImguiItem *item) : q(item) { }
};

QRhiImguiItem::QRhiImguiItem(QQuickItem *parent)
    : QQuickItem(parent),
      d(new QRhiImguiItemPrivate(this))
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
}

QRhiImguiItem::~QRhiImguiItem()
{
    delete d;
}

QSGNode *QRhiImguiItem::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *)
{
    // render thread, with main thread blocked

    if (size().isEmpty()) {
        delete node;
        return nullptr;
    }

    QRhiImguiNode *n = static_cast<QRhiImguiNode *>(node);
    if (!n)
        n = new QRhiImguiNode(d->window);

    d->gui.syncRenderer(n->renderer);

    n->markDirty(QSGNode::DirtyMaterial);
    return n;
}

void QRhiImguiItem::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &changeData)
{
    if (change == QQuickItem::ItemSceneChange) {
        if (d->window) {
            disconnect(d->windowConn);
            d->window = nullptr;
        }
        if (changeData.window) {
            d->window = window();
            d->windowConn = connect(d->window, &QQuickWindow::afterAnimating, d->window, [this] {
                if (isVisible()) {
                    d->gui.nextFrame(size(),
                                     d->window->effectiveDevicePixelRatio(),
                                     mapToScene(QPointF(0, 0)),
                                     [this] { frame(); });
                    update();
                }
            });
        }
    }
}

void QRhiImguiItem::keyPressEvent(QKeyEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::keyReleaseEvent(QKeyEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    d->gui.processEvent(event);
}

void QRhiImguiItem::mouseMoveEvent(QMouseEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::mouseReleaseEvent(QMouseEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::wheelEvent(QWheelEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::hoverMoveEvent(QHoverEvent *event)
{
    // Simulate the QWindow behavior, which means sending moves even when no
    // button is down.

    if (QGuiApplication::mouseButtons() != Qt::NoButton)
        return;

    const QPointF sceneOffset = mapToScene(event->position());
    const QPointF globalOffset = mapToGlobal(event->position());
    QMouseEvent e(QEvent::MouseMove, event->position(), event->position() + sceneOffset, event->position() + globalOffset,
                  Qt::NoButton, Qt::NoButton, QGuiApplication::keyboardModifiers());
    d->gui.processEvent(&e);
}

void QRhiImguiItem::touchEvent(QTouchEvent *event)
{
    d->gui.processEvent(event);
}

void QRhiImguiItem::frame()
{
    ImGui::ShowDemoWindow(&d->showDemoWindow);
}

QT_END_NAMESPACE
