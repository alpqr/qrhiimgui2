// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include <QOffscreenSurface>
#include <QFile>

#include "qrhiimgui.h"
#include "imgui.h"

static float vertexData[] = {
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,
};

static QShader getShader(const QString &name)
{
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

struct Window : public QWindow
{
    Window(QRhi::Implementation graphicsApi);

    void releaseSwapChain();

#if QT_CONFIG(opengl)
    std::unique_ptr<QOffscreenSurface> m_fallbackSurface;
#endif
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QRhiSwapChain> m_sc;
    std::unique_ptr<QRhiRenderBuffer> m_ds;
    std::unique_ptr<QRhiRenderPassDescriptor> m_rp;

    bool m_hasSwapChain = false;
    QMatrix4x4 m_proj;

    void init();
    void resizeSwapChain();
    void render();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    QRhi::Implementation m_graphicsApi;

    bool m_running = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    std::unique_ptr<QRhiBuffer> m_vbuf;
    bool m_vbufReady = false;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_ps;

    std::unique_ptr<QRhiImguiRenderer> m_imguiRenderer;
    QRhiImgui m_imgui;
    QMatrix4x4 m_guiMvp;

    float m_rotation = 0;
    float m_opacity = 1;
    int m_opacityDir = -1;

    void gui();
    bool m_showDemoWindow = true;
};

Window::Window(QRhi::Implementation graphicsApi)
    : m_graphicsApi(graphicsApi)
{
    switch (graphicsApi) {
    case QRhi::OpenGLES2:
        setSurfaceType(OpenGLSurface);
        break;
    case QRhi::Vulkan:
        setSurfaceType(VulkanSurface);
        break;
    case QRhi::D3D11:
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    case QRhi::D3D12:
#endif
        setSurfaceType(Direct3DSurface);
        break;
    case QRhi::Metal:
        setSurfaceType(MetalSurface);
        break;
    default:
        break;
    }
}

void Window::exposeEvent(QExposeEvent *)
{
    if (isExposed() && !m_running) {
        m_running = true;
        init();
        resizeSwapChain();
    }

    const QSize surfaceSize = m_hasSwapChain ? m_sc->surfacePixelSize() : QSize();

    if ((!isExposed() || (m_hasSwapChain && surfaceSize.isEmpty())) && m_running && !m_notExposed)
        m_notExposed = true;

    if (isExposed() && m_running && m_notExposed && !surfaceSize.isEmpty()) {
        m_notExposed = false;
        m_newlyExposed = true;
    }

    if (isExposed() && !surfaceSize.isEmpty())
        render();
}

bool Window::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::UpdateRequest:
        render();
        break;

    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseSwapChain();
        break;

    default:
        if (m_imgui.processEvent(e))
            return true;
        break;
    }

    return QWindow::event(e);
}

void Window::init()
{
    QRhi::Flags rhiFlags = QRhi::EnableDebugMarkers
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
                           | QRhi::EnableTimestamps
#else
                           | QRhi::EnableProfiling
#endif
        ;

    if (m_graphicsApi == QRhi::Null) {
        QRhiNullInitParams params;
        m_rhi.reset(QRhi::create(QRhi::Null, &params, rhiFlags));
    }

#if QT_CONFIG(opengl)
    if (m_graphicsApi == QRhi::OpenGLES2) {
        m_fallbackSurface.reset(QRhiGles2InitParams::newFallbackSurface());
        QRhiGles2InitParams params;
        params.fallbackSurface = m_fallbackSurface.get();
        params.window = this;
        m_rhi.reset(QRhi::create(QRhi::OpenGLES2, &params, rhiFlags));
    }
#endif

#if QT_CONFIG(vulkan)
    if (m_graphicsApi == QRhi::Vulkan) {
        QRhiVulkanInitParams params;
        params.inst = vulkanInstance();
        params.window = this;
        m_rhi.reset(QRhi::create(QRhi::Vulkan, &params, rhiFlags));
    }
#endif

#ifdef Q_OS_WIN
    if (m_graphicsApi == QRhi::D3D11) {
        QRhiD3D11InitParams params;
        params.enableDebugLayer = true;
        m_rhi.reset(QRhi::create(QRhi::D3D11, &params, rhiFlags));
    }
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    else if (m_graphicsApi == QRhi::D3D12) {
        QRhiD3D12InitParams params;
        params.enableDebugLayer = true;
        m_rhi.reset(QRhi::create(QRhi::D3D12, &params, rhiFlags));
    }
#endif
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    if (m_graphicsApi == QRhi::Metal) {
        QRhiMetalInitParams params;
        m_rhi.reset(QRhi::create(QRhi::Metal, &params, rhiFlags));
    }
#endif

    if (!m_rhi)
        qFatal("Failed to create RHI backend");

    m_sc.reset(m_rhi->newSwapChain());
    m_ds.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                      QSize(),
                                      1,
                                      QRhiRenderBuffer::UsedWithSwapChainOnly));
    m_sc->setWindow(this);
    m_sc->setDepthStencil(m_ds.get());
    m_rp.reset(m_sc->newCompatibleRenderPassDescriptor());
    m_sc->setRenderPassDescriptor(m_rp.get());

    m_imgui.rebuildFontAtlasWithFont(QLatin1String(":/fonts/RobotoMono-Medium.ttf"));

    m_imguiRenderer.reset(new QRhiImguiRenderer);

    m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData)));
    m_vbuf->create();
    m_vbufReady = false;

    m_ubuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68));
    m_ubuf->create();

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                                                    m_ubuf.get())
    });
    m_srb->create();

    m_ps.reset(m_rhi->newGraphicsPipeline());

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    m_ps->setTargetBlends({ premulAlphaBlend });

    const QShader vs = getShader(QLatin1String(":/shaders/color.vert.qsb"));
    if (!vs.isValid())
        qFatal("Failed to load shader pack (vertex)");
    const QShader fs = getShader(QLatin1String(":/shaders/color.frag.qsb"));
    if (!fs.isValid())
        qFatal("Failed to load shader pack (fragment)");

    m_ps->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 5 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 2 * sizeof(float) }
    });

    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb.get());
    m_ps->setRenderPassDescriptor(m_rp.get());

    m_ps->create();
}

void Window::resizeSwapChain()
{
    m_hasSwapChain = m_sc->createOrResize();

    const QSize outputSize = m_sc->currentPixelSize();
    m_proj = m_rhi->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, outputSize.width() / (float) outputSize.height(), 0.01f, 1000.0f);
    m_proj.translate(0, 0, -4);

    m_guiMvp = m_rhi->clipSpaceCorrMatrix();
    const float dpr = devicePixelRatio();
    m_guiMvp.ortho(0, outputSize.width() / dpr, outputSize.height() / dpr, 0, 1, -1);
}

void Window::releaseSwapChain()
{
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->destroy();
    }
}

void Window::render()
{
    if (!m_hasSwapChain || m_notExposed)
        return;

    if (m_sc->currentPixelSize() != m_sc->surfacePixelSize() || m_newlyExposed) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
    }

    QRhi::FrameOpResult r = m_rhi->beginFrame(m_sc.get());
    if (r == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        r = m_rhi->beginFrame(m_sc.get());
    }
    if (r != QRhi::FrameOpSuccess) {
        qDebug("beginFrame failed with %d, retry", r);
        requestUpdate();
        return;
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    QRhiRenderTarget *rt = m_sc->currentFrameRenderTarget();
    const QSize outputSizeInPixels = m_sc->currentPixelSize();

    m_imgui.nextFrame(size(), devicePixelRatio(), QPointF(0, 0), std::bind(&Window::gui, this));
    m_imgui.syncRenderer(m_imguiRenderer.get());

    m_imguiRenderer->prepare(m_rhi.get(), rt, cb, m_guiMvp, 1.0f);

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    if (!m_vbufReady) {
        m_vbufReady = true;
        u->uploadStaticBuffer(m_vbuf.get(), vertexData);
    }
    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.rotate(m_rotation, 0, 1, 0);
    u->updateDynamicBuffer(m_ubuf.get(), 0, 64, mvp.constData());
    m_opacity += m_opacityDir * 0.005f;
    if (m_opacity < 0.0f || m_opacity > 1.0f) {
        m_opacityDir *= -1;
        m_opacity = qBound(0.0f, m_opacity, 1.0f);
    }
    u->updateDynamicBuffer(m_ubuf.get(), 64, 4, &m_opacity);

    cb->beginPass(rt, QColor::fromRgbF(0.4f, 0.7f, 0.0f, 1.0f), { 1.0f, 0 }, u);

    cb->setGraphicsPipeline(m_ps.get());
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setShaderResources();

    const QRhiCommandBuffer::VertexInput vbufBinding(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(3);

    m_imguiRenderer->render();

    cb->endPass();

    m_rhi->endFrame(m_sc.get());

    requestUpdate();
}

void Window::gui()
{
    ImGuiIO &io(ImGui::GetIO());
    io.FontAllowUserScaling = true; // enable ctrl+wheel on windows
    io.IniFilename = nullptr; // no imgui.ini

    ImGui::ShowDemoWindow(&m_showDemoWindow);

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

static QString graphicsApiName(QRhi::Implementation graphicsApi)
{
    switch (graphicsApi) {
    case QRhi::Null:
        return QLatin1String("Null (no output)");
    case QRhi::OpenGLES2:
        return QLatin1String("OpenGL");
    case QRhi::Vulkan:
        return QLatin1String("Vulkan");
    case QRhi::D3D11:
        return QLatin1String("Direct3D 11");
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    case QRhi::D3D12:
        return QLatin1String("Direct3D 12");
#endif
    case QRhi::Metal:
        return QLatin1String("Metal");
    default:
        break;
    }
    return QString();
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QRhi::Implementation graphicsApi;
#if defined(Q_OS_WIN)
    graphicsApi = QRhi::D3D11;
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    graphicsApi = QRhi::Metal;
#elif QT_CONFIG(vulkan)
    graphicsApi = QRhi::Vulkan;
#else
    graphicsApi = QRhi::OpenGLES2;
#endif

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    QCommandLineOption nullOption({ "n", "null" }, QLatin1String("Null"));
    cmdLineParser.addOption(nullOption);
    QCommandLineOption glOption({ "g", "opengl" }, QLatin1String("OpenGL"));
    cmdLineParser.addOption(glOption);
    QCommandLineOption vkOption({ "v", "vulkan" }, QLatin1String("Vulkan"));
    cmdLineParser.addOption(vkOption);
    QCommandLineOption d3d11Option({ "d", "d3d11" }, QLatin1String("Direct3D 11"));
    cmdLineParser.addOption(d3d11Option);
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    QCommandLineOption d3d12Option({ "D", "d3d12" }, QLatin1String("Direct3D 12"));
    cmdLineParser.addOption(d3d12Option);
#endif
    QCommandLineOption mtlOption({ "m", "metal" }, QLatin1String("Metal"));
    cmdLineParser.addOption(mtlOption);

    cmdLineParser.process(app);
    if (cmdLineParser.isSet(nullOption))
        graphicsApi = QRhi::Null;
    if (cmdLineParser.isSet(glOption))
        graphicsApi = QRhi::OpenGLES2;
    if (cmdLineParser.isSet(vkOption))
        graphicsApi = QRhi::Vulkan;
    if (cmdLineParser.isSet(d3d11Option))
        graphicsApi = QRhi::D3D11;
#if QT_VERSION_MAJOR > 6 || QT_VERSION_MINOR >= 6
    if (cmdLineParser.isSet(d3d12Option))
        graphicsApi = QRhi::D3D12;
#endif
    if (cmdLineParser.isSet(mtlOption))
        graphicsApi = QRhi::Metal;

    qDebug("Selected graphics API is %s", qPrintable(graphicsApiName(graphicsApi)));
    qDebug("This is a multi-api example, use command line arguments to override:\n%s", qPrintable(cmdLineParser.helpText()));

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

#if QT_CONFIG(vulkan)
    QVulkanInstance inst;
    if (graphicsApi == QRhi::Vulkan) {
        inst.setLayers({ "VK_LAYER_KHRONOS_validation" });
        inst.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        if (!inst.create()) {
            qWarning("Failed to create Vulkan instance, switching to OpenGL");
            graphicsApi = QRhi::OpenGLES2;
        }
    }
#endif

    Window w(graphicsApi);
#if QT_CONFIG(vulkan)
    if (graphicsApi == QRhi::Vulkan)
        w.setVulkanInstance(&inst);
#endif
    w.resize(1280, 720);
    w.setTitle(QCoreApplication::applicationName() + QLatin1String(" - ") + graphicsApiName(graphicsApi));
    w.show();

    int ret = app.exec();

    if (w.handle())
        w.releaseSwapChain();

    return ret;
}
