// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimguiitem.h"
#include "qrhiimgui.h"
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
    QRhiImguiNode(QQuickWindow *window, QRhiImguiItem *item);
    ~QRhiImguiNode();

    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;

    QQuickWindow *window;
    QRhiImguiItem *item;
    QRhiImguiRenderer *renderer;
    QRhiImguiItemCustomRenderer *customRenderer = nullptr;
};

QRhiImguiNode::QRhiImguiNode(QQuickWindow *window, QRhiImguiItem *item)
    : window(window),
      item(item),
      renderer(new QRhiImguiRenderer)
{
    customRenderer = item->createCustomRenderer();
}

QRhiImguiNode::~QRhiImguiNode()
{
    delete customRenderer;
    delete renderer;
}

void QRhiImguiNode::releaseResources()
{
    renderer->releaseResources();
}

void QRhiImguiNode::prepare()
{
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    QRhi *rhi = window->rhi();
#else
    QSGRendererInterface *rif = window->rendererInterface();
    QRhi *rhi = static_cast<QRhi *>(rif->getResource(window, QSGRendererInterface::RhiResource));
#endif
    if (!rhi) {
        qWarning("QRhiImguiNode: No QRhi found for window %p", window);
        return;
    }

    if (customRenderer)
        customRenderer->render();

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
        n = new QRhiImguiNode(d->window, this);

    d->gui.syncRenderer(n->renderer);

    if (n->customRenderer)
        n->customRenderer->sync(n->renderer);

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
                    if (!d->window->isSceneGraphInitialized())
                        d->window->update();
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

QRhiImgui *QRhiImguiItem::imgui()
{
    return &d->gui;
}

void QRhiImguiItem::frame()
{
    ImGui::ShowDemoWindow(&d->showDemoWindow);
}

QRhiImguiItemCustomRenderer *QRhiImguiItem::createCustomRenderer()
{
    // Called on the render thread (if there is one) with the main thread blocked.

    return nullptr;
}

QRhiImguiItemCustomRenderer::~QRhiImguiItemCustomRenderer()
{
    // Called on the render thread (if there is one) when the QRhiImguiRenderer (and the scenegraph node) is going away.
    // This is convenient to safely release QRhi* objects created in sync() and customRender().
    // The QRhiImguiItem (living on the main thread) may or may not anymore exist at this point.
}

void QRhiImguiItemCustomRenderer::sync(QRhiImguiRenderer *)
{
    // Called on the render thread (if there is one) with the main thread blocked.
}

void QRhiImguiItemCustomRenderer::render()
{
    // Called on the render thread (if there is one) whenever the ImGui renderer is starting a new frame.
    // Called with a frame being recorded, but without an active render pass.
}

QT_END_NAMESPACE
