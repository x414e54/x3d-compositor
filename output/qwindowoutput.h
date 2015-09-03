#ifndef QWINDOWOUTPUT_H
#define QWINDOWOUTPUT_H

#include "opengl/opengloutput.h"
#include "input/inputlistener.h"

#include <QWindow>

class QOpenGLVertexArrayObject;

class QWindowOutput : public QWindow, public OpenGLOutput, public InputOutput
{
public:
    QWindowOutput();
    virtual ~QWindowOutput();
    virtual void init_context(QOpenGLContext* share_context);
    virtual void swap_buffers();
    virtual void make_current();
    virtual void done_current();
    virtual void get_eye_matrix(glm::mat4x4 &left, glm::mat4x4 &right);
protected:
    virtual void resizeEvent(QResizeEvent* event);
private:

    bool eventFilter(QObject *obj, QEvent *event);

    QOpenGLContext* context;
    QOpenGLVertexArrayObject* vao;
};

#endif // QWINDOWOUTPUT_H
