// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "imguiitem.h"
#include "imgui.h"

void ImguiItem::synchronize()
{
}

void ImguiItem::frame()
{
    ImGui::ShowDemoWindow(&m_showDemoWindow);
}
