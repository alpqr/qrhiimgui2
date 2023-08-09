// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QGuiApplication>
#include <QQuickView>
#include <QPainter>
#include "imguiitem.h"
#include "qrhiimgui.h"
#include "imgui.h"

ImguiItem::ImguiItem()
{
    ImGuiIO &io(ImGui::GetIO());
    io.IniFilename = nullptr;

    imgui()->rebuildFontAtlasWithFont(QLatin1String(":/fonts/RobotoMono-Medium.ttf"));
}

void ImguiItem::frame()
{
    ImGui::SetNextWindowPos(ImVec2(100, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Test", nullptr, ImGuiWindowFlags_HorizontalScrollbar);

    if (m_texture)
        ImGui::Image(m_texture, ImVec2(m_textureUnscaledWidth, m_textureUnscaledHeight));

    ImGui::End();
}

void ImguiItem::sync(QRhiImguiRenderer *renderer)
{
    // called on the render thread (if there is one) with the main thread blocked

    if (!m_texture) {
        QQuickWindow *w = window();
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
        QRhi *rhi = w->rhi();
        QRhiSwapChain *swapchain = w->swapChain();
#else
        QSGRendererInterface *rif = w->rendererInterface();
        QRhi *rhi = static_cast<QRhi *>(rif->getResource(w, QSGRendererInterface::RhiResource));
        QRhiSwapChain *swapchain = static_cast<QRhiSwapChain *>(rif->getResource(w, QSGRendererInterface::RhiSwapchainResource));
#endif
        if (rhi && swapchain) {
            m_texture = rhi->newTexture(QRhiTexture::RGBA8, QSize(512, 512), 1, QRhiTexture::RenderTarget);
            if (m_texture->create()) {
                renderer->registerCustomTexture(m_texture,
                                                m_texture,
                                                QRhiSampler::Linear,
                                                QRhiImguiRenderer::TakeCustomTextureOwnership);

                float dpr = window()->effectiveDevicePixelRatio();
                const QSize pixelSize = m_texture->pixelSize();
                const int w = pixelSize.width();
                const int h = pixelSize.height();
                m_textureUnscaledWidth = w / dpr;
                m_textureUnscaledHeight = h / dpr;

                QImage img(m_texture->pixelSize(), QImage::Format_RGBA8888);
                img.setDevicePixelRatio(dpr);
                QPainter p(&img);
                p.fillRect(0, 0, m_textureUnscaledWidth, m_textureUnscaledHeight, Qt::red);
                QFont font;
                font.setPointSize(16);
                p.setFont(font);
                p.drawText(10, 50, QLatin1String("Hello world with QPainter in a texture"));
                QPen pen(Qt::yellow);
                pen.setWidth(4);
                p.setPen(pen);
                p.drawRect(0, 0, m_textureUnscaledWidth - 1, m_textureUnscaledHeight - 1);
                p.end();

                QRhiCommandBuffer *cb = swapchain->currentFrameCommandBuffer();
                // or could instead do cb->beginPass() etc.
                QRhiResourceUpdateBatch *u = rhi->nextResourceUpdateBatch();
                u->uploadTexture(m_texture, img);
                cb->resourceUpdate(u);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qputenv("QSG_INFO", "1");

    QQuickView view;
    view.setColor(Qt::black);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1280, 720);
    view.setSource(QUrl("qrc:/main.qml"));
    view.show();

    int r = app.exec();

    return r;
}
