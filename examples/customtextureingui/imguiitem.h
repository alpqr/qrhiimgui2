// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef IMGUIITEM_H
#define IMGUIITEM_H

#include "qrhiimguiitem.h"

class QRhiTexture;

class ImguiItem : public QRhiImguiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Imgui)

public:
    ImguiItem();
    void frame() override;
    void sync(QRhiImguiRenderer *renderer) override;

private:
    QRhiTexture *m_texture = nullptr;
    int m_textureUnscaledWidth = 0;
    int m_textureUnscaledHeight = 0;
};

#endif
