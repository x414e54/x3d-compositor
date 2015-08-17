#include "qwindowoutput.h"

#include <QOpenGLContext>
#include <QtGui/QOpenGLFunctions_3_2_Core>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QResizeEvent>

QWindowOutput::QWindowOutput() : context(nullptr)
{
    QSurfaceFormat format;
    format.setMajorVersion(3);
    format.setMinorVersion(2);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    setFormat(format);
    setSurfaceType(OpenGLSurface);
    create();

    this->quad_buffered = this->format().stereo();
    this->output_width = this->width();
    this->output_height = this->height();
}


QWindowOutput::~QWindowOutput()
{

}

void QWindowOutput::init_context(QOpenGLContext* share_context)
{
    if (context != nullptr) {
        if (share_context == context->shareContext()) {
            return;
        }

        context->makeCurrent(this);
        if (vao != nullptr) {
            delete vao;
        }
        if (gl != nullptr) {
            delete gl;
        }
        context->doneCurrent();
        delete context;
    }

    context = new QOpenGLContext();
    context->setShareContext(share_context);
    context->setFormat(requestedFormat());
    if (!context->create()) {
        throw;
    }

    context->makeCurrent(this);
    gl = context->versionFunctions<QOpenGLFunctions_3_2_Core>();
    if (gl == nullptr || !gl->initializeOpenGLFunctions()) {
        throw;
    }
    vao = new QOpenGLVertexArrayObject();
    vao->bind();

    context->doneCurrent();
}

void QWindowOutput::swap_buffers()
{
    ScopedOutputContext context(*this);
    this->context->swapBuffers(this);
}

void QWindowOutput::make_current()
{
    if (context == nullptr) {
        throw;
    }
    context->makeCurrent(this);
}

void QWindowOutput::done_current()
{
    if (context == nullptr) {
        throw;
    }
    context->doneCurrent();
}

void QWindowOutput::resizeEvent(QResizeEvent* event)
{
   QWindow::resizeEvent(event);
   this->output_height = event->size().width();
   this->output_width = event->size().height();
}
