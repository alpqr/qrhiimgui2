// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "qrhiimgui_p.h"
#include <QFile>
#include <QMouseEvent>
#include <QKeyEvent>

QT_BEGIN_NAMESPACE

QRhiImgui::QRhiImgui()
    : d(new QRhiImguiPrivate)
{
}

QRhiImgui::~QRhiImgui()
{
    releaseResources();
    delete d;
}

void QRhiImgui::setFrameFunc(FrameFunc f)
{
    d->frame = f;
}

static QShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());

    return QShader();
}

// the imgui default
static_assert(sizeof(ImDrawVert) == 20);
// switched to uint in imconfig.h to avoid trouble with 4 byte offset alignment reqs
static_assert(sizeof(ImDrawIdx) == 4);

// all rendering (initialize, prepare, record, release) happens on the render thread

bool QRhiImgui::prepareFrame(QRhiRenderTarget *rt, QRhiResourceUpdateBatch *dstResourceUpdates)
{
    ImGuiIO &io(ImGui::GetIO());

    if (d->textures.isEmpty()) {
        unsigned char *pixels;
        int w, h;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        const QImage wrapperImg(const_cast<const uchar *>(pixels), w, h, QImage::Format_RGBA8888);
        QRhiImguiPrivate::Texture t;
        t.image = wrapperImg.copy();
        d->textures.append(t);
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(quintptr(d->textures.count() - 1)));
    }

    d->rt = rt;

    const QSize outputSize = rt->pixelSize();
    io.DisplaySize.x = outputSize.width();
    io.DisplaySize.y = outputSize.height();
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    ImGui::NewFrame();
    if (d->frame)
        d->frame();

    ImGui::Render();

    if (!d->ubuf) {
        d->ubuf = d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 4);
        d->ubuf->setName(QByteArrayLiteral("imgui uniform buffer"));
        d->releasePool << d->ubuf;
        if (!d->ubuf->create())
            return false;

        float opacity = 1.0f;
        dstResourceUpdates->updateDynamicBuffer(d->ubuf, 64, 4, &opacity);
    }

    if (d->lastOutputSize != outputSize) {
        QMatrix4x4 mvp = d->rhi->clipSpaceCorrMatrix();
        mvp.ortho(0, io.DisplaySize.x, io.DisplaySize.y, 0, 1, -1);
        dstResourceUpdates->updateDynamicBuffer(d->ubuf, 0, 64, mvp.constData());
        d->lastOutputSize = outputSize;
    }

    if (!d->sampler) {
        d->sampler = d->rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                        QRhiSampler::Repeat, QRhiSampler::Repeat);
        d->sampler->setName(QByteArrayLiteral("imgui sampler"));
        d->releasePool << d->sampler;
        if (!d->sampler->create())
            return false;
    }

    for (int i = 0; i < d->textures.count(); ++i) {
        QRhiImguiPrivate::Texture &t(d->textures[i]);
        if (!t.tex) {
            t.tex = d->rhi->newTexture(QRhiTexture::RGBA8, t.image.size());
            t.tex->setName(QByteArrayLiteral("imgui texture ") + QByteArray::number(i));
            if (!t.tex->create())
                return false;
            dstResourceUpdates->uploadTexture(t.tex, t.image);
        }
        if (!t.srb) {
            t.srb = d->rhi->newShaderResourceBindings();
            t.srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d->ubuf),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, t.tex, d->sampler)
            });
            if (!t.srb->create())
                return false;
        }
    }

    if (!d->ps) {
        d->ps = d->rhi->newGraphicsPipeline();
        d->releasePool << d->ps;
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::Zero;
        blend.colorWrite = QRhiGraphicsPipeline::R | QRhiGraphicsPipeline::G | QRhiGraphicsPipeline::B;
        d->ps->setTargetBlends({ blend });
        d->ps->setCullMode(QRhiGraphicsPipeline::None);
        d->ps->setDepthTest(d->depthTest);
        d->ps->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        d->ps->setDepthWrite(false);
        d->ps->setFlags(QRhiGraphicsPipeline::UsesScissor);

        QShader vs = getShader(QLatin1String(":/imgui.vert.qsb"));
        Q_ASSERT(vs.isValid());
        QShader fs = getShader(QLatin1String(":/imgui.frag.qsb"));
        Q_ASSERT(fs.isValid());
        d->ps->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fs }
        });

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 4 * sizeof(float) + sizeof(quint32) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
            { 0, 2, QRhiVertexInputAttribute::UNormByte4, 4 * sizeof(float) }
        });

        d->ps->setVertexInputLayout(inputLayout);
        d->ps->setShaderResourceBindings(d->textures[0].srb);
        d->ps->setRenderPassDescriptor(rt->renderPassDescriptor());

        if (!d->ps->create())
            return false;
    }

    ImDrawData *draw = ImGui::GetDrawData();
    d->vbufOffsets.resize(draw->CmdListsCount);
    d->ibufOffsets.resize(draw->CmdListsCount);
    quint32 totalVbufSize = 0;
    quint32 totalIbufSize = 0;
    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const int vbufSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
        d->vbufOffsets[n] = totalVbufSize;
        totalVbufSize += vbufSize;
        const int ibufSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
        d->ibufOffsets[n] = totalIbufSize;
        totalIbufSize += ibufSize;
    }

    if (!d->vbuf) {
        d->vbuf = d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, totalVbufSize);
        d->vbuf->setName(QByteArrayLiteral("imgui vertex buffer"));
        d->releasePool << d->vbuf;
        if (!d->vbuf->create())
            return false;
    } else {
        if (totalVbufSize > d->vbuf->size()) {
            d->vbuf->setSize(totalVbufSize);
            if (!d->vbuf->create())
                return false;
        }
    }
    if (!d->ibuf) {
        d->ibuf = d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, totalIbufSize);
        d->ibuf->setName(QByteArrayLiteral("imgui index buffer"));
        d->releasePool << d->ibuf;
        if (!d->ibuf->create())
            return false;
    } else {
        if (totalIbufSize > d->ibuf->size()) {
            d->ibuf->setSize(totalIbufSize);
            if (!d->ibuf->create())
                return false;
        }
    }

    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const quint32 vbufSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
        dstResourceUpdates->updateDynamicBuffer(d->vbuf, d->vbufOffsets[n], vbufSize, cmdList->VtxBuffer.Data);
        const quint32 ibufSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
        dstResourceUpdates->updateDynamicBuffer(d->ibuf, d->ibufOffsets[n], ibufSize, cmdList->IdxBuffer.Data);
    }

    return true;
}

void QRhiImgui::recordFrame(QRhiCommandBuffer *cb)
{
    QRhiCommandBuffer::VertexInput vbufBinding(d->vbuf, 0);
    bool needsViewport = true;
    const int outputHeight = d->rt->pixelSize().height();

    ImDrawData *draw = ImGui::GetDrawData();
    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const ImDrawIdx *indexBufOffset = nullptr;
        vbufBinding.second = d->vbufOffsets[n];

        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            const quint32 indexOffset = d->ibufOffsets[n] + quintptr(indexBufOffset);

            if (!cmd->UserCallback) {
                cb->setGraphicsPipeline(d->ps);
                if (needsViewport) {
                    needsViewport = false;
                    cb->setViewport({ 0, 0, float(d->lastOutputSize.width()), float(d->lastOutputSize.height()) });
                }
                QPoint sp = QPointF(cmd->ClipRect.x, outputHeight - cmd->ClipRect.w).toPoint();
                QSize ss = QSizeF(cmd->ClipRect.z - cmd->ClipRect.x, cmd->ClipRect.w - cmd->ClipRect.y).toSize();
                sp.setX(qMax(0, sp.x()));
                sp.setY(qMax(0, sp.y()));
                ss.setWidth(qMin(d->lastOutputSize.width(), ss.width()));
                ss.setHeight(qMin(d->lastOutputSize.height(), ss.height()));
                cb->setScissor({ sp.x(), sp.y(), ss.width(), ss.height() });
                const int textureIndex = int(reinterpret_cast<qintptr>(cmd->TextureId));
                cb->setShaderResources(d->textures[textureIndex].srb);
                cb->setVertexInput(0, 1, &vbufBinding, d->ibuf, indexOffset, QRhiCommandBuffer::IndexUInt32);
                cb->drawIndexed(cmd->ElemCount);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }

            indexBufOffset += cmd->ElemCount;
        }
    }
}

void QRhiImgui::initialize(QRhi *rhi)
{
    ImGui::CreateContext();
    d->rhi = rhi;
    d->lastOutputSize = QSize();
}

void QRhiImgui::releaseResources()
{
    if (!ImGui::GetCurrentContext())
        return;

    for (QRhiImguiPrivate::Texture &t : d->textures) {
        delete t.tex;
        delete t.srb;
    }
    d->textures.clear();

    qDeleteAll(d->releasePool);
    d->releasePool.clear();

    d->vbuf = d->ibuf = d->ubuf = nullptr;
    d->ps = nullptr;
    d->sampler = nullptr;

    d->rhi = nullptr;

    ImGui::DestroyContext();
}

void QRhiImgui::setDepthTest(bool enabled)
{
    d->depthTest = enabled;
}

QRhiImgui::FrameFunc QRhiImgui::frameFunc() const
{
    return d->frame;
}

// input stuff (event filtering, updateInput) happens on the main thread

void QRhiImgui::setInputEventSource(QObject *src, QObject *filterOwner)
{
    if (d->inputEventSource && d->inputEventFilter)
        d->inputEventSource->removeEventFilter(d->inputEventFilter);

    d->inputEventSource = src;

    if (!d->inputEventFilter) {
        d->inputEventFilter = new QRhiImGuiInputEventFilter(filterOwner);
        d->inputInitialized = false;
    }

    d->inputEventSource->installEventFilter(d->inputEventFilter);
}

void QRhiImgui::setEatInputEvents(bool enabled)
{
    if (d->inputEventFilter)
        d->inputEventFilter->eatEvents = enabled;
}

bool QRhiImGuiInputEventFilter::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        mousePos = me->position();
        mouseButtonsDown = me->buttons();
        modifiers = me->modifiers();
    }
        return eatEvents;

    case QEvent::Wheel:
    {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        mouseWheel += we->angleDelta().y() / 120.0f;
    }
        return eatEvents;

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        const bool down = event->type() == QEvent::KeyPress;
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        modifiers = ke->modifiers();
        if (down)
            keyText.append(ke->text());
        int k = ke->key();
        if (k <= 0xFF)
            keyDown[k] = down;
        else if (k >= FIRSTSPECKEY && k <= LASTSPECKEY)
            keyDown[MAPSPECKEY(k)] = down;
    }
        return eatEvents;

    default:
        break;
    }

    return false;
}

void QRhiImgui::updateInput(const QPointF &logicalOffset, float dpr)
{
    if (!d->inputEventFilter || !ImGui::GetCurrentContext())
        return;

    ImGuiIO &io = ImGui::GetIO();

    if (!d->inputInitialized) {
        d->inputInitialized = true;

        io.KeyMap[ImGuiKey_Tab] = MAPSPECKEY(Qt::Key_Tab);
        io.KeyMap[ImGuiKey_LeftArrow] = MAPSPECKEY(Qt::Key_Left);
        io.KeyMap[ImGuiKey_RightArrow] = MAPSPECKEY(Qt::Key_Right);
        io.KeyMap[ImGuiKey_UpArrow] = MAPSPECKEY(Qt::Key_Up);
        io.KeyMap[ImGuiKey_DownArrow] = MAPSPECKEY(Qt::Key_Down);
        io.KeyMap[ImGuiKey_PageUp] = MAPSPECKEY(Qt::Key_PageUp);
        io.KeyMap[ImGuiKey_PageDown] = MAPSPECKEY(Qt::Key_PageDown);
        io.KeyMap[ImGuiKey_Home] = MAPSPECKEY(Qt::Key_Home);
        io.KeyMap[ImGuiKey_End] = MAPSPECKEY(Qt::Key_End);
        io.KeyMap[ImGuiKey_Delete] = MAPSPECKEY(Qt::Key_Delete);
        io.KeyMap[ImGuiKey_Backspace] = MAPSPECKEY(Qt::Key_Backspace);
        io.KeyMap[ImGuiKey_Enter] = MAPSPECKEY(Qt::Key_Return);
        io.KeyMap[ImGuiKey_Escape] = MAPSPECKEY(Qt::Key_Escape);

        io.KeyMap[ImGuiKey_A] = Qt::Key_A;
        io.KeyMap[ImGuiKey_C] = Qt::Key_C;
        io.KeyMap[ImGuiKey_V] = Qt::Key_V;
        io.KeyMap[ImGuiKey_X] = Qt::Key_X;
        io.KeyMap[ImGuiKey_Y] = Qt::Key_Y;
        io.KeyMap[ImGuiKey_Z] = Qt::Key_Z;
    }

    QRhiImGuiInputEventFilter *w = d->inputEventFilter;

    const QPointF pos = (w->mousePos + logicalOffset) * dpr;
    io.MousePos = ImVec2(pos.x(), pos.y());

    io.MouseDown[0] = w->mouseButtonsDown.testFlag(Qt::LeftButton);
    io.MouseDown[1] = w->mouseButtonsDown.testFlag(Qt::RightButton);
    io.MouseDown[2] = w->mouseButtonsDown.testFlag(Qt::MiddleButton);

    io.MouseWheel = w->mouseWheel;
    w->mouseWheel = 0;

    io.KeyCtrl = w->modifiers.testFlag(Qt::ControlModifier);
    io.KeyShift = w->modifiers.testFlag(Qt::ShiftModifier);
    io.KeyAlt = w->modifiers.testFlag(Qt::AltModifier);
    io.KeySuper = w->modifiers.testFlag(Qt::MetaModifier);

    memcpy(io.KeysDown, w->keyDown, sizeof(w->keyDown));

    if (!w->keyText.isEmpty()) {
        for (const QChar &c : qAsConst(w->keyText)) {
            ImWchar u = c.unicode();
            if (u)
                io.AddInputCharacter(u);
        }
        w->keyText.clear();
    }
}

QT_END_NAMESPACE
