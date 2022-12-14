// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef IMGUIITEM_H
#define IMGUIITEM_H

#include "qrhiimguiitem.h"

class ImguiItem : public QRhiImguiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Imgui)

public:
    QVector<std::function<void()>> callbacks;
    void frame() override {
        for (auto &f : callbacks)
            f();
    }
};

#endif
