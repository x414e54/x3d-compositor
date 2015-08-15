#ifndef QWINDOWOUTPUT_H
#define QWINDOWOUTPUT_H

#include "opengloutput.h"
#include <QWindow>

class QWindowOutput : public QWindow, public OpenGLOutput
{
public:
    QWindowOutput();
    virtual ~QWindowOutput();
    virtual void swapBuffers();
private:
    QOpenGLContext* context;
};

#endif // QWINDOWOUTPUT_H
