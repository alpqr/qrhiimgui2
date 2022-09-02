// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimguiitem.h"
#include "qrhiimguinode_p.h"
#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickwindow.h>
#include <QtGui/qimage.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qclipboard.h>

#include "imgui.h"

// the imgui default
static_assert(sizeof(ImDrawVert) == 20);
// switched to uint in imconfig.h to avoid trouble with 4 byte offset alignment reqs
static_assert(sizeof(ImDrawIdx) == 4);

QT_BEGIN_NAMESPACE

struct QRhiImguiItemPrivate
{
    QRhiImguiItem *q;
    QQuickWindow *window = nullptr;
    QMetaObject::Connection windowConn;
    QRhiImguiNode::StaticRenderData sf;
    QRhiImguiNode::FrameRenderData f;
    Qt::MouseButtons pressedMouseButtons;
    bool showDemoWindow = true;

    QRhiImguiItemPrivate(QRhiImguiItem *item) : q(item) { }
    void nextImguiFrame();
    void processEvent(QEvent *event);
};

static const char *getClipboardText(void *)
{
    static QByteArray contents;
    contents = QGuiApplication::clipboard()->text().toUtf8();
    return contents.constData();
}

static void setClipboardText(void *, const char *text)
{
    QGuiApplication::clipboard()->setText(QString::fromUtf8(text));
}

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
    d->sf.fontTextureData = wrapperImg.copy();
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(quintptr(0)));

    io.GetClipboardTextFn = getClipboardText;
    io.SetClipboardTextFn = setClipboardText;
}

QRhiImguiItem::~QRhiImguiItem()
{
    ImGui::DestroyContext();
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

    n->sf = d->sf;
    n->f = std::move(d->f);

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

    const float dpr = window->effectiveDevicePixelRatio();
    f.outputPixelSize = (q->size() * dpr).toSize();
    const QPointF itemPixelOffset = q->mapToScene(QPointF(0, 0)) * dpr;

    io.DisplaySize.x = q->width();
    io.DisplaySize.y = q->height();
    io.DisplayFramebufferScale = ImVec2(dpr, dpr);

    ImGui::NewFrame();
    q->frame();
    ImGui::Render();

    ImDrawData *draw = ImGui::GetDrawData();
    draw->ScaleClipRects(ImVec2(dpr, dpr));

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
                QRhiImguiNode::DrawCmd dc;
                dc.cmdListBufferIdx = n;
                dc.textureIndex = int(reinterpret_cast<qintptr>(cmd->TextureId));
                dc.indexOffset = indexOffset;
                dc.elemCount = cmd->ElemCount;
                dc.itemPixelOffset = itemPixelOffset;
                dc.clipRect = QVector4D(cmd->ClipRect.x, cmd->ClipRect.y, cmd->ClipRect.z, cmd->ClipRect.w);
                f.draw.append(dc);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }
            indexBufOffset += cmd->ElemCount;
        }
    }
}

static void updateKeyboardModifiers(Qt::KeyboardModifiers modifiers)
{
    ImGuiIO &io(ImGui::GetIO());
    io.AddKeyEvent(ImGuiKey_ModCtrl, modifiers.testFlag(Qt::ControlModifier));
    io.AddKeyEvent(ImGuiKey_ModShift, modifiers.testFlag(Qt::ShiftModifier));
    io.AddKeyEvent(ImGuiKey_ModAlt, modifiers.testFlag(Qt::AltModifier));
    io.AddKeyEvent(ImGuiKey_ModSuper, modifiers.testFlag(Qt::MetaModifier));
}

static ImGuiKey mapKey(int k)
{
    switch (k) {
    case Qt::Key_Space:
        return ImGuiKey_Space;
    case Qt::Key_Apostrophe:
        return ImGuiKey_Apostrophe;
    case Qt::Key_Comma:
        return ImGuiKey_Comma;
    case Qt::Key_Minus:
        return ImGuiKey_Minus;
    case Qt::Key_Period:
        return ImGuiKey_Period;
    case Qt::Key_Slash:
        return ImGuiKey_Slash;
    case Qt::Key_0:
        return ImGuiKey_0;
    case Qt::Key_1:
        return ImGuiKey_1;
    case Qt::Key_2:
        return ImGuiKey_2;
    case Qt::Key_3:
        return ImGuiKey_3;
    case Qt::Key_4:
        return ImGuiKey_4;
    case Qt::Key_5:
        return ImGuiKey_5;
    case Qt::Key_6:
        return ImGuiKey_6;
    case Qt::Key_7:
        return ImGuiKey_8;
    case Qt::Key_8:
        return ImGuiKey_8;
    case Qt::Key_9:
        return ImGuiKey_9;
    case Qt::Key_Semicolon:
        return ImGuiKey_Semicolon;
    case Qt::Key_Equal:
        return ImGuiKey_Equal;
    case Qt::Key_A:
        return ImGuiKey_A;
    case Qt::Key_B:
        return ImGuiKey_B;
    case Qt::Key_C:
        return ImGuiKey_C;
    case Qt::Key_D:
        return ImGuiKey_D;
    case Qt::Key_E:
        return ImGuiKey_E;
    case Qt::Key_F:
        return ImGuiKey_F;
    case Qt::Key_G:
        return ImGuiKey_G;
    case Qt::Key_H:
        return ImGuiKey_H;
    case Qt::Key_I:
        return ImGuiKey_I;
    case Qt::Key_J:
        return ImGuiKey_J;
    case Qt::Key_K:
        return ImGuiKey_K;
    case Qt::Key_L:
        return ImGuiKey_L;
    case Qt::Key_M:
        return ImGuiKey_M;
    case Qt::Key_N:
        return ImGuiKey_N;
    case Qt::Key_O:
        return ImGuiKey_O;
    case Qt::Key_P:
        return ImGuiKey_P;
    case Qt::Key_Q:
        return ImGuiKey_Q;
    case Qt::Key_R:
        return ImGuiKey_R;
    case Qt::Key_S:
        return ImGuiKey_S;
    case Qt::Key_T:
        return ImGuiKey_T;
    case Qt::Key_U:
        return ImGuiKey_U;
    case Qt::Key_V:
        return ImGuiKey_V;
    case Qt::Key_W:
        return ImGuiKey_W;
    case Qt::Key_X:
        return ImGuiKey_X;
    case Qt::Key_Y:
        return ImGuiKey_Y;
    case Qt::Key_Z:
        return ImGuiKey_Z;
    case Qt::Key_BracketLeft:
        return ImGuiKey_LeftBracket;
    case Qt::Key_Backslash:
        return ImGuiKey_Backslash;
    case Qt::Key_BracketRight:
        return ImGuiKey_RightBracket;
    case Qt::Key_QuoteLeft:
        return ImGuiKey_GraveAccent;
    case Qt::Key_Escape:
        return ImGuiKey_Escape;
    case Qt::Key_Tab:
        return ImGuiKey_Tab;
    case Qt::Key_Backspace:
        return ImGuiKey_Backspace;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return ImGuiKey_Enter;
    case Qt::Key_Insert:
        return ImGuiKey_Insert;
    case Qt::Key_Delete:
        return ImGuiKey_Delete;
    case Qt::Key_Pause:
        return ImGuiKey_Pause;
    case Qt::Key_Print:
        return ImGuiKey_PrintScreen;
    case Qt::Key_Home:
        return ImGuiKey_Home;
    case Qt::Key_End:
        return ImGuiKey_End;
    case Qt::Key_Left:
        return ImGuiKey_LeftArrow;
    case Qt::Key_Up:
        return ImGuiKey_UpArrow;
    case Qt::Key_Right:
        return ImGuiKey_RightArrow;
    case Qt::Key_Down:
        return ImGuiKey_DownArrow;
    case Qt::Key_PageUp:
        return ImGuiKey_PageUp;
    case Qt::Key_PageDown:
        return ImGuiKey_PageDown;
    case Qt::Key_Shift:
        return ImGuiKey_LeftShift;
    case Qt::Key_Control:
        return ImGuiKey_LeftCtrl;
    case Qt::Key_Meta:
        return ImGuiKey_LeftSuper;
    case Qt::Key_Alt:
        return ImGuiKey_LeftAlt;
    case Qt::Key_CapsLock:
        return ImGuiKey_CapsLock;
    case Qt::Key_NumLock:
        return ImGuiKey_NumLock;
    case Qt::Key_ScrollLock:
        return ImGuiKey_ScrollLock;
    case Qt::Key_F1:
        return ImGuiKey_F1;
    case Qt::Key_F2:
        return ImGuiKey_F2;
    case Qt::Key_F3:
        return ImGuiKey_F3;
    case Qt::Key_F4:
        return ImGuiKey_F4;
    case Qt::Key_F5:
        return ImGuiKey_F5;
    case Qt::Key_F6:
        return ImGuiKey_F6;
    case Qt::Key_F7:
        return ImGuiKey_F7;
    case Qt::Key_F8:
        return ImGuiKey_F8;
    case Qt::Key_F9:
        return ImGuiKey_F9;
    case Qt::Key_F10:
        return ImGuiKey_F10;
    case Qt::Key_F11:
        return ImGuiKey_F11;
    case Qt::Key_F12:
        return ImGuiKey_F12;
    default:
        break;
    }
    return ImGuiKey_None;
}

void QRhiImguiItemPrivate::processEvent(QEvent *event)
{
    ImGuiIO &io(ImGui::GetIO());

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        updateKeyboardModifiers(me->modifiers());
        Qt::MouseButtons buttons = me->buttons();
        if (buttons.testFlag(Qt::LeftButton) && !pressedMouseButtons.testFlag(Qt::LeftButton))
            io.AddMouseButtonEvent(0, true);
        if (buttons.testFlag(Qt::RightButton) && !pressedMouseButtons.testFlag(Qt::RightButton))
            io.AddMouseButtonEvent(1, true);
        if (buttons.testFlag(Qt::MiddleButton) && !pressedMouseButtons.testFlag(Qt::MiddleButton))
            io.AddMouseButtonEvent(2, true);
        pressedMouseButtons = buttons;
   }
        break;

    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        Qt::MouseButtons buttons = me->buttons();
        if (!buttons.testFlag(Qt::LeftButton) && pressedMouseButtons.testFlag(Qt::LeftButton))
            io.AddMouseButtonEvent(0, false);
        if (!buttons.testFlag(Qt::RightButton) && pressedMouseButtons.testFlag(Qt::RightButton))
            io.AddMouseButtonEvent(1, false);
        if (!buttons.testFlag(Qt::MiddleButton) && pressedMouseButtons.testFlag(Qt::MiddleButton))
            io.AddMouseButtonEvent(2, false);
        pressedMouseButtons = buttons;
    }
        break;

    case QEvent::MouseMove:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        const QPointF pos = me->position();
        io.AddMousePosEvent(pos.x(), pos.y());
    }
        break;

    case QEvent::Wheel:
    {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        QPointF wheel(we->angleDelta().x() / 120.0f, we->angleDelta().y() / 120.0f);
        io.AddMouseWheelEvent(wheel.x(), wheel.y());
    }
        break;

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        const bool down = event->type() == QEvent::KeyPress;
        updateKeyboardModifiers(ke->modifiers());
        io.AddKeyEvent(mapKey(ke->key()), down);
        if (down && !ke->text().isEmpty()) {
            const QByteArray text = ke->text().toUtf8();
            io.AddInputCharactersUTF8(text.constData());
        }
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
