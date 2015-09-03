/****************************************************************************
**
** Copyright (C) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#if USE_COMPOSITOR

#include "qwindowcompositor.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QTouchEvent>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QGuiApplication>
#include <QCursor>
#include <QPixmap>
#include <QLinkedList>
#include <QScreen>
#include <QPainter>

#include <QtCompositor/qwaylandinput.h>
#include <QtCompositor/qwaylandbufferref.h>
#include <QtCompositor/qwaylandsurfaceview.h>
#include <QtCompositor/qwaylandoutput.h>

#include "output/qwindowoutput.h"

QT_BEGIN_NAMESPACE

class BufferAttacher : public QWaylandBufferAttacher
{
public:
    BufferAttacher(OpenGLOutput* output)
        : QWaylandBufferAttacher()
        , shmTex(0)
        , output(output)
    {
    }

    ~BufferAttacher()
    {
        delete shmTex;
    }

    void attach(const QWaylandBufferRef &ref) Q_DECL_OVERRIDE
    {
        ScopedOutputContext context(*output);

        if (bufferRef) {
            if (bufferRef.isShm()) {
                delete shmTex;
                shmTex = 0;
            } else {
                bufferRef.destroyTexture();
            }
        }

        bufferRef = ref;

        if (bufferRef) {
            if (bufferRef.isShm()) {
                shmTex = new QOpenGLTexture(bufferRef.image(), QOpenGLTexture::DontGenerateMipMaps);
                shmTex->setWrapMode(QOpenGLTexture::ClampToEdge);
                texture = shmTex->textureId();
            } else {
                texture = bufferRef.createTexture();
            }
        }
    }

    QImage image() const
    {
        if (!bufferRef || !bufferRef.isShm())
            return QImage();
        return bufferRef.image();
    }

    QOpenGLTexture *shmTex;
    QWaylandBufferRef bufferRef;
    GLuint texture;
    OpenGLOutput* output;
};

static QRect mm_to_pixels(const QRect& in)
{
    const int ppcm = 47; // Get from surface info later.

    const float ppm = ppcm / 10.0f;

    QRect  out(in.x() * ppm,
               in.y() * ppm,
               in.width() * ppm,
               in.height() * ppm);
    return out;
}


QWindowCompositor::QWindowCompositor(QWindowOutput *window, X3DScene *scene)
    : QWaylandCompositor(0, DefaultExtensions | SubSurfaceExtension | XDGShellExtension)
    , m_window(window)
    , m_scene(scene)
    , m_renderScheduler(this)
    , m_updateScheduler(this)
    , m_modifiers(Qt::NoModifier)
{
    m_renderScheduler.setSingleShot(true);
    connect(&m_renderScheduler,SIGNAL(timeout()),this,SLOT(render()));

    m_updateScheduler.setSingleShot(false);
    m_updateScheduler.setInterval(10);
    connect(&m_updateScheduler,SIGNAL(timeout()),this,SLOT(update()));

    m_scene->installEventFilter(this);

    setRetainedSelectionEnabled(true);

    createOutput(nullptr, "X3D", "Roaming");
    QRect mm_size = QRect(0, 0, 1000.0, (1000.0 / 16.0) * 9.0);
    primaryOutput()->setPhysicalSize(QSize(mm_size.width(), mm_size.height()));
    primaryOutput()->setGeometry(mm_to_pixels(mm_size));
    addDefaultShell();

    m_updateScheduler.start();
}

QWindowCompositor::~QWindowCompositor()
{
}

void QWindowCompositor::surfaceDestroyed()
{
    QWaylandSurface *surface = static_cast<QWaylandSurface *>(sender());

    foreach (QWaylandSurfaceView *view, surface->views()) {
        m_scene->remove_texture(view);
    }

    m_surfaces.removeOne(surface);
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceMapped()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    QPoint pos;
    if (!m_surfaces.contains(surface)) {
        if (surface->windowType() != QWaylandSurface::Popup) {
            uint px = 0;
            uint py = 0;
            if (!QCoreApplication::arguments().contains(QLatin1String("-stickytopleft"))) {
                px = 1 + (qrand() % (m_window->width() - surface->size().width() - 2));
                py = 1 + (qrand() % (m_window->height() - surface->size().height() - 2));
            }
            pos = QPoint(px, py);
            QWaylandSurfaceView *view = surface->views().first();
            view->setPos(pos);
        }
    } else {
        m_surfaces.removeOne(surface);
    }

    if (surface->windowType() == QWaylandSurface::Popup) {
        QWaylandSurfaceView *view = surface->views().first();
        view->setPos(surface->transientParent()->views().first()->pos() + surface->transientOffset());
    }

    m_surfaces.append(surface);

    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceUnmapped()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());

    foreach (QWaylandSurfaceView *view, surface->views()) {
        m_scene->remove_texture(view);
    }

    if (m_surfaces.removeOne(surface))
        m_surfaces.insert(0, surface);

    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceCommitted()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    surfaceCommitted(surface);
}

void QWindowCompositor::surfacePosChanged()
{
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceCommitted(QWaylandSurface *surface)
{
    Q_UNUSED(surface)
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceCreated(QWaylandSurface *surface)
{
    connect(surface, SIGNAL(surfaceDestroyed()), this, SLOT(surfaceDestroyed()));
    connect(surface, SIGNAL(mapped()), this, SLOT(surfaceMapped()));
    connect(surface, SIGNAL(unmapped()), this, SLOT(surfaceUnmapped()));
    connect(surface, SIGNAL(redraw()), this, SLOT(surfaceCommitted()));
    connect(surface, SIGNAL(extendedSurfaceReady()), this, SLOT(sendExpose()));
    m_renderScheduler.start(0);

    surface->setBufferAttacher(new BufferAttacher(m_window));
}

void QWindowCompositor::sendExpose()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    surface->sendOnScreenVisibilityChange(true);
}

static QRectF pixels_to_m(const QRect& in)
{
    const int ppcm = 47; // Get from surface info later.

    const float ppm = ppcm * 100.0f;

    QRectF out(in.x() / ppm,
              in.y() / ppm,
              in.width() / ppm,
              in.height() / ppm);
    return out;
}

void QWindowCompositor::update()
{
    m_scene->update();
    m_renderScheduler.start(0);
}

void QWindowCompositor::render()
{
    frameStarted();

    cleanupGraphicsResources();

    foreach (QWaylandSurface *surface, m_surfaces) {
        if (!surface->visible())
            continue;
        GLuint texture = static_cast<BufferAttacher *>(surface->bufferAttacher())->texture;
        foreach (QWaylandSurfaceView *view, surface->views()) {
            QRectF geo = pixels_to_m(QRect(view->pos().toPoint(), surface->size()));
            m_scene->add_texture(texture, geo.width(), geo.height(),
                                 surface->size().width(), surface->size().height(), view);
            foreach (QWaylandSurface *child, surface->subSurfaces()) {
                drawSubSurface(view->pos().toPoint(), child);
            }
        }
    }

    m_scene->render(m_window->size()->width(), m_window->size()->height());

    sendFrameCallbacks(surfaces());

    m_window->swap_buffers();
}

void QWindowCompositor::drawSubSurface(const QPoint &offset, QWaylandSurface *surface)
{
    GLuint texture = static_cast<BufferAttacher *>(surface->bufferAttacher())->texture;
    QWaylandSurfaceView *view = surface->views().first();
    QPoint pos = view->pos().toPoint() + offset;
    QRectF geo = pixels_to_m(QRect(pos, surface->size()));
    m_scene->add_texture(texture, geo.width(), geo.height(),
                         surface->size().width(), surface->size().height(), view);
    foreach (QWaylandSurface *child, surface->subSurfaces()) {
        drawSubSurface(pos, child);
    }
}

bool QWindowCompositor::sceneKeyEventFilter(void *obj, int key, SceneEvent state)
{
    QWaylandInputDevice *input = defaultInputDevice();
    QWaylandSurfaceView *target = static_cast<QWaylandSurfaceView*>(obj);
    if (target && target->surface())
    {
        if (state == EXIT && target->surface() == input->keyboardFocus()) {
            input->setKeyboardFocus(nullptr);
        } else if (target->surface() != input->keyboardFocus()) {
            input->setKeyboardFocus(target->surface());
        }

        if (input->keyboardFocus()) {
            if (state == DOWN) {
                input->sendKeyPressEvent(key);
            } else if (state == UP) {
                input->sendKeyReleaseEvent(key);
            }
        }

        return true;
    }
    return false;
}

bool QWindowCompositor::sceneEventFilter(void *obj, const float (&pos)[2], SceneEvent state)
{
    QWaylandInputDevice *input = defaultInputDevice();
    QWaylandSurfaceView *target = static_cast<QWaylandSurfaceView*>(obj);
    if (target && target->surface())
    {
        QSize size = target->surface()->size();
        QPointF point(pos[0] * size.width(), pos[1] * size.height());

        if (state == EXIT && target == input->mouseFocus()) {
            input->setMouseFocus(nullptr, point, point);
        } if (target != input->mouseFocus()) {
            input->setMouseFocus(target, point, point);
        }

        if (input->mouseFocus()) {
            if (state == DOWN) {
                input->sendMousePressEvent(Qt::LeftButton, point, point);
                input->sendTouchPointEvent(0, point.x(), point.y(), Qt::TouchPointPressed);
            } else if (state == UP) {
                input->sendMouseReleaseEvent(Qt::LeftButton, point, point);
                input->sendTouchPointEvent(0, point.x(), point.y(), Qt::TouchPointReleased);
            } else if (state == DRAG || state == OVER) {
                input->sendMouseMoveEvent(point, point);
                input->sendTouchPointEvent(0, point.x(), point.y(), Qt::TouchPointMoved);
            }
        }

        return true;
    }
    return false;
}


QT_END_NAMESPACE

#endif
