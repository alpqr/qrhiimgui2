// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUIITEM_H
#define QRHIIMGUIITEM_H

#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

class QRhiImguiItem : public QQuickItem
{
    Q_OBJECT

public:
    QRhiImguiItem(QQuickItem *parent = nullptr);
    ~QRhiImguiItem();

    // Called on the render thread with main thread blocked to allow copying
    // data from main thread data structures.
    virtual void synchronize();

    // Called on the render thread, this is where the ImGui UI should be
    // generated based on the data stored in synchronize().
    virtual void frame();

private:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void itemChange(QQuickItem::ItemChange, const QQuickItem::ItemChangeData &) override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void touchEvent(QTouchEvent *event) override;

    QQuickWindow *m_window = nullptr;
    QMetaObject::Connection m_windowConn;
    QObject m_dummy;
};

QT_END_NAMESPACE

#endif
