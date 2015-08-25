/****************************************************************************
**
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

#ifndef QWINDOWCOMPOSITOR_H
#define QWINDOWCOMPOSITOR_H

#include "qwaylandcompositor.h"
#include "qwaylandsurface.h"
#include "x3d/x3dscene.h"

#include <QtGui/private/qopengltexturecache_p.h>
#include <QObject>
#include <QTimer>

QT_BEGIN_NAMESPACE

class QWaylandSurfaceView;
class QOpenGLTexture;
class QWindowOutput;

class QWindowCompositor : public QObject, public QWaylandCompositor, public SceneEventFilter
{
    Q_OBJECT
public:
    QWindowCompositor(QWindowOutput *window, X3DScene *scene);
    ~QWindowCompositor();

private slots:
    void surfaceDestroyed();
    void surfaceMapped();
    void surfaceUnmapped();
    void surfaceCommitted();
    void surfacePosChanged();

    void render();
    void update();
protected:
    void surfaceCommitted(QWaylandSurface *surface);
    void surfaceCreated(QWaylandSurface *surface);
    virtual bool sceneEventFilter(void *, const float (&pos)[2], SceneEvent);
    bool eventFilter(QObject *obj, QEvent *event);

    void ensureKeyboardFocusSurface(QWaylandSurface *oldSurface);

    virtual bool openUrl(QWaylandClient *client, const QUrl &url) {}
private slots:
    void sendExpose();

private:
    void drawSubSurface(const QPoint &offset, QWaylandSurface *surface);

    QWindowOutput *m_window;
    QList<QWaylandSurface *> m_surfaces;
    X3DScene *m_scene;
    GLuint m_surface_fbo;
    QTimer m_renderScheduler;
    QTimer m_updateScheduler;

    Qt::KeyboardModifiers m_modifiers;
};

QT_END_NAMESPACE

#endif // QWINDOWCOMPOSITOR_H
