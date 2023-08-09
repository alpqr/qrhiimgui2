// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QGuiApplication>
#include <QQuickView>
#include <QPainter>
#include "triangle.h"
#include "imguiitem.h"
#include "qrhiimgui.h"

#include "imgui.h"

ImguiItem::ImguiItem()
{
    ImGuiIO &io(ImGui::GetIO());
    io.IniFilename = nullptr;

    imgui()->rebuildFontAtlasWithFont(QLatin1String(":/fonts/RobotoMono-Medium.ttf"));
}

static inline ImVec2 unscaledSize(const CustomRenderer::CustomContent &c)
{
    const QSize pixelSize = c.texture->pixelSize();
    return ImVec2(pixelSize.width() / c.dpr, pixelSize.height() / c.dpr);
}

void ImguiItem::frame()
{
    ImGui::ShowDemoWindow(&demoWindowOpen);

    ImGui::SetNextWindowPos(ImVec2(100, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("QPainter -> QImage -> QRhiTexture -> ImGui::Image()", nullptr, ImGuiWindowFlags_HorizontalScrollbar);

    if (cr && cr->swPainted.texture)
        ImGui::Image(cr->swPainted.texture, unscaledSize(cr->swPainted));

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(500, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("QRhi-based rendering to a QRhiTexture -> Imgui::Image()", nullptr, ImGuiWindowFlags_HorizontalScrollbar);

    if (cr && cr->rhiRendered.texture)
        ImGui::Image(cr->rhiRendered.texture, unscaledSize(cr->rhiRendered));

    ImGui::End();
}

QRhiImguiItemCustomRenderer *ImguiItem::createCustomRenderer()
{
    // called on the render thread (if there is one) with the main thread blocked

    cr = new CustomRenderer(window());
    return cr;
}

void CustomRenderer::sync(QRhiImguiRenderer *renderer)
{
    // called on the render thread (if there is one) with the main thread blocked

#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    QRhi *rhi = window->rhi();
#else
    QSGRendererInterface *rif = window->rendererInterface();
    QRhi *rhi = static_cast<QRhi *>(rif->getResource(window, QSGRendererInterface::RhiResource));
#endif

    if (rhi) {
        if (!swPainted.texture) {
            swPainted.texture = rhi->newTexture(QRhiTexture::RGBA8, QSize(512, 512));
            if (swPainted.texture->create()) {
                renderer->registerCustomTexture(swPainted.texture,
                                                swPainted.texture,
                                                QRhiSampler::Linear,
                                                QRhiImguiRenderer::TakeCustomTextureOwnership);
                swPainted.dpr = window->effectiveDevicePixelRatio();
            }
        }
        if (!rhiRendered.texture) {
            rhiRendered.texture = rhi->newTexture(QRhiTexture::RGBA8, QSize(512, 512), 1, QRhiTexture::RenderTarget);
            if (rhiRendered.texture->create()) {
                renderer->registerCustomTexture(rhiRendered.texture,
                                                rhiRendered.texture,
                                                QRhiSampler::Linear,
                                                QRhiImguiRenderer::TakeCustomTextureOwnership);
                rhiRendered.dpr = window->effectiveDevicePixelRatio();
                triRt = rhi->newTextureRenderTarget({ rhiRendered.texture });
                triRpDesc = triRt->newCompatibleRenderPassDescriptor();
                triRt->setRenderPassDescriptor(triRpDesc);
                triRt->create();
            }
        }
    }
}

void CustomRenderer::render()
{
    // called on the render thread (if there is one)

#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    QRhi *rhi = window->rhi();
    QRhiSwapChain *swapchain = window->swapChain();
#else
    QSGRendererInterface *rif = window->rendererInterface();
    QRhi *rhi = static_cast<QRhi *>(rif->getResource(window, QSGRendererInterface::RhiResource));
    QRhiSwapChain *swapchain = static_cast<QRhiSwapChain *>(rif->getResource(window, QSGRendererInterface::RhiSwapchainResource));
#endif
    if (!rhi || !swapchain)
        return;

    // ----- swPainted ----- (draw with QPainter into a QImage -> upload to QRhiTexture)

    QImage img(swPainted.texture->pixelSize(), QImage::Format_RGBA8888);
    img.setDevicePixelRatio(swPainted.dpr);

    const int textureUnscaledWidth = img.width() / swPainted.dpr;
    const int textureUnscaledHeight = img.height() / swPainted.dpr;

    QPainter p(&img);
    p.fillRect(0, 0, textureUnscaledWidth, textureUnscaledHeight, Qt::red);
    QFont font;
    font.setPointSize(16);
    p.setFont(font);
    p.drawText(10, 50, QLatin1String("Hello world with QPainter in a texture"));
    QPen pen(Qt::yellow);
    pen.setWidth(4);
    p.setPen(pen);
    p.drawRect(0, 0, textureUnscaledWidth - 1, textureUnscaledHeight - 1);
    p.end();

    QRhiCommandBuffer *cb = swapchain->currentFrameCommandBuffer();
    QRhiResourceUpdateBatch *u = rhi->nextResourceUpdateBatch();
    u->uploadTexture(swPainted.texture, img);
    cb->resourceUpdate(u);

    // ----- rhiRendered ----- (render with QRhi into the QRhiTexture)

    if (!triangleRenderer) {
        triangleRenderer = new Triangle;
        triangleRenderer->init(rhi, cb, triRpDesc);
    }

    triangleRenderer->render(cb, triRt, Qt::transparent, triRotation, 1.0f);
    triRotation += 1.0f;
}

CustomRenderer::~CustomRenderer()
{
    // called on the render thread (if there is one)

    delete triangleRenderer;
    delete triRt;
    delete triRpDesc;
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
