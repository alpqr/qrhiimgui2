// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimguiitem.h"
#include "qrhiimguinode_p.h"
#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickwindow.h>
#include <QtGui/qimage.h>
#include <QtGui/qmatrix4x4.h>

#include "imgui.h"

// the imgui default
static_assert(sizeof(ImDrawVert) == 20);
// switched to uint in imconfig.h to avoid trouble with 4 byte offset alignment reqs
static_assert(sizeof(ImDrawIdx) == 4);

QT_BEGIN_NAMESPACE

// allow mapping the range Qt::Key_Escape..Qt::Key_PageDown
#define FIRSTSPECKEY (0x01000000)
#define LASTSPECKEY (0x01000017)
#define MAPSPECKEY(k) ((k) - FIRSTSPECKEY + 256)

struct QRhiImguiItemPrivate
{
    QRhiImguiItem *q;
    QQuickWindow *window = nullptr;
    QMetaObject::Connection windowConn;
    QSize lastOutputSize;
    QRhiImguiNode::FrameRenderData f;

    bool inputInitialized = false;
    QPointF mousePos;
    Qt::MouseButtons mouseButtonsDown = Qt::NoButton;
    float mouseWheel = 0;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    bool keyDown[256 + (LASTSPECKEY - FIRSTSPECKEY + 1)] = {};
    QString keyText;

    bool showDemoWindow = true;

    QRhiImguiItemPrivate(QRhiImguiItem *item) : q(item) { }
    void nextImguiFrame();
    void updateInput(const QPointF &logicalOffset, float dpr);
    void processEvent(QEvent *event);
};

QRhiImguiItem::QRhiImguiItem(QQuickItem *parent)
    : QQuickItem(parent),
      d(new QRhiImguiItemPrivate(this))
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);

    ImGui::CreateContext();
    ImGuiIO &io(ImGui::GetIO());
    unsigned char *pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    const QImage wrapperImg(const_cast<const uchar *>(pixels), w, h, QImage::Format_RGBA8888);
    d->f.fontTextureData = wrapperImg.copy();
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(quintptr(0)));
}

QRhiImguiItem::~QRhiImguiItem()
{
    ImGui::DestroyContext();
    delete d;
}

QSGNode *QRhiImguiItem::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *)
{
    // render thread, with main thread blocked

    QRhiImguiNode *n = static_cast<QRhiImguiNode *>(node);
    if (!n)
        n = new QRhiImguiNode(d->window);

    n->m_itemSize = size();
    n->f = d->f;

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
                    d->updateInput(mapToScene(QPointF(0, 0)), d->window->effectiveDevicePixelRatio());
                    d->nextImguiFrame();
                    update();
                }
            });
        }
    }
}

void QRhiImguiItemPrivate::nextImguiFrame()
{
    ImGuiIO &io(ImGui::GetIO());

    const QSize outputSize = (q->size() * window->effectiveDevicePixelRatio()).toSize();
    io.DisplaySize.x = outputSize.width();
    io.DisplaySize.y = outputSize.height();
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    ImGui::NewFrame();
    q->frame();
    ImGui::Render();

    if (lastOutputSize != outputSize) {
        f.mvp = QMatrix4x4();
        f.mvp.ortho(0, io.DisplaySize.x, io.DisplaySize.y, 0, 1, -1);
        lastOutputSize = outputSize;
    }

    ImDrawData *draw = ImGui::GetDrawData();
    f.vbuf.resize(draw->CmdListsCount);
    f.ibuf.resize(draw->CmdListsCount);
    f.totalVbufSize = 0;
    f.totalIbufSize = 0;
    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const int vbufSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
        f.vbuf[n].offset = f.totalVbufSize;
        f.totalVbufSize += vbufSize;
        const int ibufSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
        f.ibuf[n].offset = f.totalIbufSize;
        f.totalIbufSize += ibufSize;
    }
    f.draw.clear();
    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        f.vbuf[n].data = QByteArray(reinterpret_cast<const char *>(cmdList->VtxBuffer.Data),
                                    cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        f.ibuf[n].data = QByteArray(reinterpret_cast<const char *>(cmdList->IdxBuffer.Data),
                                    cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        const ImDrawIdx *indexBufOffset = nullptr;
        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            const quint32 indexOffset = f.ibuf[n].offset + quintptr(indexBufOffset);
            if (!cmd->UserCallback) {
                QPoint sp = QPointF(cmd->ClipRect.x, outputSize.height() - cmd->ClipRect.w).toPoint();
                QSize ss = QSizeF(cmd->ClipRect.z - cmd->ClipRect.x, cmd->ClipRect.w - cmd->ClipRect.y).toSize();
                sp.setX(qMax(0, sp.x()));
                sp.setY(qMax(0, sp.y()));
                ss.setWidth(qMin(outputSize.width(), ss.width()));
                ss.setHeight(qMin(outputSize.height(), ss.height()));
                QRhiImguiNode::DrawCmd dc;
                dc.cmdListBufferIdx = n;
                dc.indexOffset = indexOffset;
                dc.textureIndex = int(reinterpret_cast<qintptr>(cmd->TextureId));
                dc.scissorBottomLeft = sp;
                dc.scissorSize = ss;
                dc.elemCount = cmd->ElemCount;
                f.draw.append(dc);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }
            indexBufOffset += cmd->ElemCount;
        }
    }
}

void QRhiImguiItemPrivate::updateInput(const QPointF &logicalOffset, float dpr)
{
    if (!ImGui::GetCurrentContext())
        return;

    ImGuiIO &io = ImGui::GetIO();

    if (!inputInitialized) {
        inputInitialized = true;

        io.KeyMap[ImGuiKey_Tab] = MAPSPECKEY(Qt::Key_Tab);
        io.KeyMap[ImGuiKey_LeftArrow] = MAPSPECKEY(Qt::Key_Left);
        io.KeyMap[ImGuiKey_RightArrow] = MAPSPECKEY(Qt::Key_Right);
        io.KeyMap[ImGuiKey_UpArrow] = MAPSPECKEY(Qt::Key_Up);
        io.KeyMap[ImGuiKey_DownArrow] = MAPSPECKEY(Qt::Key_Down);
        io.KeyMap[ImGuiKey_PageUp] = MAPSPECKEY(Qt::Key_PageUp);
        io.KeyMap[ImGuiKey_PageDown] = MAPSPECKEY(Qt::Key_PageDown);
        io.KeyMap[ImGuiKey_Home] = MAPSPECKEY(Qt::Key_Home);
        io.KeyMap[ImGuiKey_End] = MAPSPECKEY(Qt::Key_End);
        io.KeyMap[ImGuiKey_Delete] = MAPSPECKEY(Qt::Key_Delete);
        io.KeyMap[ImGuiKey_Backspace] = MAPSPECKEY(Qt::Key_Backspace);
        io.KeyMap[ImGuiKey_Enter] = MAPSPECKEY(Qt::Key_Return);
        io.KeyMap[ImGuiKey_Escape] = MAPSPECKEY(Qt::Key_Escape);

        // just to make Ctrl+A and such working, should be extended
        io.KeyMap[ImGuiKey_A] = Qt::Key_A;
        io.KeyMap[ImGuiKey_C] = Qt::Key_C;
        io.KeyMap[ImGuiKey_V] = Qt::Key_V;
        io.KeyMap[ImGuiKey_X] = Qt::Key_X;
        io.KeyMap[ImGuiKey_Y] = Qt::Key_Y;
        io.KeyMap[ImGuiKey_Z] = Qt::Key_Z;
    }

    const QPointF pos = (mousePos + logicalOffset) * dpr;
    io.MousePos = ImVec2(pos.x(), pos.y());

    io.MouseDown[0] = mouseButtonsDown.testFlag(Qt::LeftButton);
    io.MouseDown[1] = mouseButtonsDown.testFlag(Qt::RightButton);
    io.MouseDown[2] = mouseButtonsDown.testFlag(Qt::MiddleButton);

    io.MouseWheel = mouseWheel;
    mouseWheel = 0;

    io.KeyCtrl = modifiers.testFlag(Qt::ControlModifier);
    io.KeyShift = modifiers.testFlag(Qt::ShiftModifier);
    io.KeyAlt = modifiers.testFlag(Qt::AltModifier);
    io.KeySuper = modifiers.testFlag(Qt::MetaModifier);

    memcpy(io.KeysDown, keyDown, sizeof(keyDown));

    if (!keyText.isEmpty()) {
        for (const QChar &c : qAsConst(keyText)) {
            ImWchar u = c.unicode();
            if (u)
                io.AddInputCharacter(u);
        }
        keyText.clear();
    }
}

void QRhiImguiItemPrivate::processEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        mousePos = me->position();
        mouseButtonsDown = me->buttons();
        modifiers = me->modifiers();
    }
        break;

    case QEvent::Wheel:
    {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        mouseWheel += we->angleDelta().y() / 120.0f;
    }
        break;

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        const bool down = event->type() == QEvent::KeyPress;
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        modifiers = ke->modifiers();
        if (down)
            keyText.append(ke->text());
        int k = ke->key();
        if (k <= 0xFF)
            keyDown[k] = down;
        else if (k >= FIRSTSPECKEY && k <= LASTSPECKEY)
            keyDown[MAPSPECKEY(k)] = down;
    }
        break;

    default:
        break;
    }
}

void QRhiImguiItem::keyPressEvent(QKeyEvent *event)
{
    d->processEvent(event);
}

void QRhiImguiItem::keyReleaseEvent(QKeyEvent *event)
{
    d->processEvent(event);
}

void QRhiImguiItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    d->processEvent(event);
}

void QRhiImguiItem::mouseMoveEvent(QMouseEvent *event)
{
    d->processEvent(event);
}

void QRhiImguiItem::mouseReleaseEvent(QMouseEvent *event)
{
    d->processEvent(event);
}

void QRhiImguiItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    d->processEvent(event);
}

void QRhiImguiItem::wheelEvent(QWheelEvent *event)
{
    d->processEvent(event);
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
    d->processEvent(&e);
}

void QRhiImguiItem::touchEvent(QTouchEvent *event)
{
    d->processEvent(event);
}

void QRhiImguiItem::frame()
{
    ImGui::ShowDemoWindow(&d->showDemoWindow);
}

QT_END_NAMESPACE
