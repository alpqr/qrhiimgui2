// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QGuiApplication>
#include <QQuickView>
#include <QMutex>
#include "imguiitem.h"
#include "imgui.h"

namespace Test {
static bool showDemoWindow = true;

static void frame()
{
    ImGuiIO &io(ImGui::GetIO());
    io.FontAllowUserScaling = true; // enable ctrl+wheel on windows
    io.IniFilename = nullptr; // no imgui.ini

    ImGui::ShowDemoWindow(&showDemoWindow);

    ImGui::SetNextWindowPos(ImVec2(50, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Test");
    static char s[512];
    bool print = false;
    if (ImGui::InputText("qDebug", s, sizeof(s), ImGuiInputTextFlags_EnterReturnsTrue))
        print = true;
    if (ImGui::Button("Print"))
        print = true;
    if (print)
        qDebug("%s", s);
    ImGui::End();
}

} // namespace Test

namespace LogWin {
static QtMessageHandler prevMsgHandler;
static QMutex msgMutex;
static QStringList msgBuf;
static bool msgBufChanged = false;
const int MAX_LOG_MESSAGE_LENGTH = 1000;
const int MAX_LOG_LENGTH = 10000;
static bool scrollOnChange = true;

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (prevMsgHandler)
        prevMsgHandler(type, ctx, msg);

    QMutexLocker locker(&msgMutex);

    QString decoratedMsg;
    if (ctx.category) {
        decoratedMsg += QString::fromUtf8(ctx.category);
        decoratedMsg += QLatin1String(": ");
    }
    decoratedMsg += QStringView(msg).left(MAX_LOG_MESSAGE_LENGTH);

    while (msgBuf.count() > MAX_LOG_LENGTH)
        msgBuf.removeFirst();

    msgBuf.append(decoratedMsg);
    msgBufChanged = true;
}

static bool logWindowOpen = true;

static void frame()
{
    if (!logWindowOpen)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Log", &logWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    QMutexLocker locker(&msgMutex);

    if (ImGui::Button("Clear"))
        msgBuf.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Scroll on change", &scrollOnChange);
    ImGui::Separator();

    ImGui::BeginChild("loglist", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const QString &msg : msgBuf) {
        const QByteArray msgBa = msg.toUtf8() + '\n';
        ImGui::TextUnformatted(msgBa.constData());
    }
    if (scrollOnChange && msgBufChanged)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::End();

    msgBufChanged = false;
}

} // namespace LogWin

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qputenv("QSG_INFO", "1");
    LogWin::prevMsgHandler = qInstallMessageHandler(LogWin::messageHandler);

    QQuickView view;
    view.setColor(Qt::black);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1280, 720);
    view.setSource(QUrl("qrc:/main.qml"));
    view.show();

    ImguiItem *gui = view.rootObject()->findChild<ImguiItem *>("gui");
    gui->callbacks << Test::frame << LogWin::frame;

    int r = app.exec();

    return r;
}
