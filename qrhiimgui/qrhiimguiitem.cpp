// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimguiitem.h"
#include "qrhiimguinode_p.h"
#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/private/qsgrendernode_p.h>

// the QSGNode lives entirely on the render thread
QRhiImguiNode::QRhiImguiNode(QQuickWindow *window)
    : m_window(window)
{
}

QRhiImguiNode::~QRhiImguiNode()
{
}

void QRhiImguiNode::initialize()
{
    QSGRendererInterface *rif = m_window->rendererInterface();
    m_rhi = static_cast<QRhi *>(rif->getResource(m_window, QSGRendererInterface::RhiResource));
    if (!m_rhi) {
        qWarning("No QRhi found for window %p, QRhiImguiItem will not be functional", m_window);
        return;
    }
    m_imgui.initialize(m_rhi);
}

void QRhiImguiNode::releaseResources()
{
    m_imgui.releaseResources();
}

void QRhiImguiNode::prepare()
{
    if (!m_rhi)
        initialize();
    if (!m_rhi)
        return;

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    QSGRenderNodePrivate *d = QSGRenderNodePrivate::get(this);
    m_imgui.prepareFrame(d->m_rt.rt, u);
    d->m_rt.cb->resourceUpdate(u);
}

void QRhiImguiNode::render(const RenderState *)
{
    if (!m_rhi)
        return;

    QSGRenderNodePrivate *d = QSGRenderNodePrivate::get(this);
    m_imgui.recordFrame(d->m_rt.cb);
}

QSGRenderNode::StateFlags QRhiImguiNode::changedStates() const
{
    return ScissorState | BlendState | DepthState | CullState;
}

QSGRenderNode::RenderingFlags QRhiImguiNode::flags() const
{
    return BoundedRectRendering | DepthAwareRendering;
}

QRectF QRhiImguiNode::rect() const
{
    return QRect(0, 0, m_itemSize.width(), m_itemSize.height());
}

QRhiImguiItem::QRhiImguiItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
}

QRhiImguiItem::~QRhiImguiItem()
{
}

QSGNode *QRhiImguiItem::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *)
{
    // render thread, with main blocked

    QRhiImguiNode *n = static_cast<QRhiImguiNode *>(node);
    if (!n) {
        n = new QRhiImguiNode(m_window);
        n->m_imgui.setFrameFunc([this] { frame(); });
        QMetaObject::invokeMethod(this, [this, n] {
            // This is meant to be used on a QWindow but any QObject to which we
            // forward events will do. Being a true QQuickItem has the benefit of
            // proper keyboard focus, so no need to bother with setEatInputEvents.
            n->m_imgui.setInputEventSource(&m_dummy, this);
        }, Qt::QueuedConnection);
    }

    n->m_itemSize = size();
    n->m_imgui.updateInput(mapToScene(QPointF(0, 0)), m_window->effectiveDevicePixelRatio());
    synchronize();

    n->markDirty(QSGNode::DirtyMaterial);
    return n;
}

void QRhiImguiItem::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &changeData)
{
    if (change == QQuickItem::ItemSceneChange) {
        if (m_window) {
            disconnect(m_windowConn);
            m_window = nullptr;
        }
        if (changeData.window) {
            m_window = window();

            // the imgui "render loop"
            m_windowConn = connect(m_window, &QQuickWindow::afterAnimating, m_window, [this] {
                if (isVisible()) {
                    // triggers continuous updates as long as visible
                    update();
                }
            });
        }
    }
}

void QRhiImguiItem::keyPressEvent(QKeyEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::keyReleaseEvent(QKeyEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::mouseMoveEvent(QMouseEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::mouseReleaseEvent(QMouseEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::wheelEvent(QWheelEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
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
    QCoreApplication::sendEvent(&m_dummy, &e);
}

void QRhiImguiItem::touchEvent(QTouchEvent *event)
{
    QCoreApplication::sendEvent(&m_dummy, event);
}

void QRhiImguiItem::synchronize()
{
}

void QRhiImguiItem::frame()
{
}
