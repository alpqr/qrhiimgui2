// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "imguiitem.h"
#include "imgui.h"

void ImguiItem::frame()
{
    ImGuiIO &io(ImGui::GetIO());
    io.FontAllowUserScaling = true; // enable ctrl+wheel on windows

    QRhiImguiItem::frame(); // demo window, no need to call it if that's not wanted

    ImGui::SetNextWindowPos(ImVec2(200, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Test");
    ImGui::End();
}
