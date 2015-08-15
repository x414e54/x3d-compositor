#ifndef QWINDOWOUTPUT_H
#define QWINDOWOUTPUT_H

#include "opengl/opengloutput.h"
#include <QWindow>

class QOpenGLVertexArrayObject;

class QWindowOutput : public QWindow, public OpenGLOutput
{
public:
    QWindowOutput();
    virtual ~QWindowOutput();
    virtual void init_context(QOpenGLContext* share_context);
    virtual void swap_buffers();
    virtual void make_current();
    virtual void done_current();
private:
    QOpenGLContext* context;
    QOpenGLVertexArrayObject* vao;
};

#endif // QWINDOWOUTPUT_H
