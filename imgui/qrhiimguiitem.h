// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef QRHIIMGUIITEM_H
#define QRHIIMGUIITEM_H

#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

struct QRhiImguiItemPrivate;
class QRhiImguiRenderer;

class QRhiImguiItem : public QQuickItem
{
    Q_OBJECT

public:
    QRhiImguiItem(QQuickItem *parent = nullptr);
    ~QRhiImguiItem();

    virtual void frame();
    virtual void sync(QRhiImguiRenderer *renderer);

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

    QRhiImguiItemPrivate *d;
};

QT_END_NAMESPACE

#endif
