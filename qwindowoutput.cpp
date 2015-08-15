#include "qwindowoutput.h"

#include <QOpenGLContext>

QWindowOutput::QWindowOutput()
{
    setSurfaceType(OpenGLSurface);

    QSurfaceFormat format;
    setFormat(format);
    create();

    context = new QOpenGLContext();
    context->setFormat(format);
    context->create();
}


QWindowOutput::~QWindowOutput()
{

}

void QWindowOutput::swapBuffers()
{
    context->swapBuffers(this);
}
