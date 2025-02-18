// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QApplication>
#include <QCommandLineParser>
#include <QRhiWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QFont>
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

class Widget : public QRhiWidget
{
public:
    Widget(QWidget *parent = nullptr);

    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;

    bool event(QEvent *e) override;

private:
    void gui();

    QRhi *m_rhi = nullptr;
    QMatrix4x4 m_proj;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_ps;

    std::unique_ptr<QRhiImguiRenderer> m_imguiRenderer;
    QRhiImgui m_imgui;
    QMatrix4x4 m_guiMvp;

    float m_rotation = 0;
    float m_opacity = 1;
    int m_opacityDir = -1;

    bool m_showDemoWindow = true;
};

Widget::Widget(QWidget *parent)
    : QRhiWidget(parent)
{
    // to get mouse moves even when no button is pressed, i.e. to match the QWindow behavior
    setMouseTracking(true);
}

void Widget::initialize(QRhiCommandBuffer *cb)
{
    if (!m_imguiRenderer) {
        ImGuiIO &io(ImGui::GetIO());
        io.IniFilename = nullptr;
        m_imgui.rebuildFontAtlasWithFont(QLatin1String(":/fonts/RobotoMono-Medium.ttf"));
        m_imguiRenderer.reset(new QRhiImguiRenderer);
    }
    if (m_rhi != rhi()) {
        m_ps.reset();
        m_rhi = rhi();
    }
    if (!m_ps) {
        m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData)));
        m_vbuf->create();

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

        m_ps->setShaderStages({
            { QRhiShaderStage::Vertex, getShader(QLatin1String(":/shaders/color.vert.qsb")) },
            { QRhiShaderStage::Fragment, getShader(QLatin1String(":/shaders/color.frag.qsb")) }
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
        m_ps->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

        m_ps->create();

        QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();
        resourceUpdates->uploadStaticBuffer(m_vbuf.get(), vertexData);
        cb->resourceUpdate(resourceUpdates);
    }

    const QSize outputSize = renderTarget()->pixelSize();
    m_proj = m_rhi->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, outputSize.width() / (float) outputSize.height(), 0.01f, 1000.0f);
    m_proj.translate(0, 0, -4);

    m_guiMvp = m_rhi->clipSpaceCorrMatrix();
    const float dpr = devicePixelRatio();
    m_guiMvp.ortho(0, outputSize.width() / dpr, outputSize.height() / dpr, 0, 1, -1);
}

void Widget::render(QRhiCommandBuffer *cb)
{
    m_imgui.nextFrame(size(), devicePixelRatio(), QPointF(0, 0), std::bind(&Widget::gui, this));
    m_imgui.syncRenderer(m_imguiRenderer.get());

    m_imguiRenderer->prepare(m_rhi, renderTarget(), cb, m_guiMvp, 1.0f);

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
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

    const QSize outputSizeInPixels = renderTarget()->pixelSize();
    cb->beginPass(renderTarget(), QColor::fromRgbF(0.4f, 0.7f, 0.0f, 1.0f), { 1.0f, 0 }, u);

    cb->setGraphicsPipeline(m_ps.get());
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setShaderResources();

    const QRhiCommandBuffer::VertexInput vbufBinding(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(3);

    m_imguiRenderer->render();

    cb->endPass();

    update();
}

void Widget::gui()
{
    ImGui::ShowDemoWindow(&m_showDemoWindow);

    ImGui::SetNextWindowPos(ImVec2(50, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Test");
    ImGui::Text("QRhiWidget (with QRhi backend %s)\nin a QScrollArea in a QVBoxLayout", rhi()->backendName());
    ImGui::End();
}

bool Widget::event(QEvent *e)
{
    if (m_imgui.processEvent(e))
        return true;

    return QRhiWidget::event(e);
}

struct ScrollAreaKeyFilter : public QObject
{
    ScrollAreaKeyFilter(QObject *target) : m_target(target) { }
    bool eventFilter(QObject *obj, QEvent *e) override {
        switch (e->type()) {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            QCoreApplication::sendEvent(m_target, e);
            return true;
        default:
            break;
        }
        return QObject::eventFilter(obj, e);
    }
    QObject *m_target;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QRhiWidget::Api graphicsApi;
#if defined(Q_OS_WIN)
    graphicsApi = QRhiWidget::Api::Direct3D11;
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    graphicsApi = QRhiWidget::Api::Metal;
#elif QT_CONFIG(vulkan)
    graphicsApi = QRhiWidget::Api::Vulkan;
#else
    graphicsApi = QRhiWidget::Api::OpenGL;
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
    QCommandLineOption d3d12Option({ "D", "d3d12" }, QLatin1String("Direct3D 12"));
    cmdLineParser.addOption(d3d12Option);
    QCommandLineOption mtlOption({ "m", "metal" }, QLatin1String("Metal"));
    cmdLineParser.addOption(mtlOption);

    cmdLineParser.process(app);
    if (cmdLineParser.isSet(nullOption))
        graphicsApi = QRhiWidget::Api::Null;
    if (cmdLineParser.isSet(glOption))
        graphicsApi = QRhiWidget::Api::OpenGL;
    if (cmdLineParser.isSet(vkOption))
        graphicsApi = QRhiWidget::Api::Vulkan;
    if (cmdLineParser.isSet(d3d11Option))
        graphicsApi = QRhiWidget::Api::Direct3D11;
    if (cmdLineParser.isSet(d3d12Option))
        graphicsApi = QRhiWidget::Api::Direct3D12;
    if (cmdLineParser.isSet(mtlOption))
        graphicsApi = QRhiWidget::Api::Metal;

    QVBoxLayout *layout = new QVBoxLayout;

    QScrollArea *scrollArea = new QScrollArea;
    Widget *rhiWidget = new Widget;
    rhiWidget->resize(1920, 1080);
    rhiWidget->setApi(graphicsApi);
    qDebug() << rhiWidget->api();
    scrollArea->setWidget(rhiWidget);
    layout->addWidget(scrollArea);

    ScrollAreaKeyFilter scrollAreaKeyFilter(rhiWidget);
    scrollArea->installEventFilter(&scrollAreaKeyFilter);

    QVBoxLayout *otherContent = new QVBoxLayout;
    otherContent->addWidget(new QLabel(QLatin1String("A label")));
    otherContent->addWidget(new QPushButton(QLatin1String("A button")));
    QCheckBox *cb = new QCheckBox(QLatin1String("Show widget a widget on top"));
    cb->setChecked(true);
    otherContent->addWidget(cb);
    layout->addLayout(otherContent);

    QWidget topLevel;
    topLevel.setLayout(layout);
    topLevel.resize(1280, 720);

    QLabel *overlayLabel = new QLabel(&topLevel);
    overlayLabel->setText(QLatin1String("Some QWidget on top\n(to prove stacking and translucency works)"));
    overlayLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    overlayLabel->setAutoFillBackground(true);
    QPalette semiTransparent(QColor(255, 0, 0, 64));
    semiTransparent.setBrush(QPalette::Text, Qt::white);
    semiTransparent.setBrush(QPalette::WindowText, Qt::white);
    overlayLabel->setPalette(semiTransparent);
    QFont f = overlayLabel->font();
    f.setPixelSize(QFontInfo(f).pixelSize() * 2);
    f.setWeight(QFont::Bold);
    overlayLabel->setFont(f);
    overlayLabel->resize(600, 150);
    overlayLabel->move(200, 500);

    QObject::connect(cb, &QCheckBox::stateChanged, overlayLabel, [overlayLabel, cb] {
        overlayLabel->setVisible(cb->isChecked());
    });

    topLevel.show();
    return app.exec();
}
