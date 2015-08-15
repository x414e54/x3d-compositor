#include "openglrenderer.h"

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions_4_3_Compatibility>
#include <QFuture>

ContextPoolContext::ContextPoolContext()
{
    QSurfaceFormat format;
    setFormat(format);
    create();

    context = new QOpenGLContext();
    context->setFormat(format);
    context->create();
}

bool ContextPoolContext::make_current()
{
    if (used.testAndSetAcquire(0, 1)) {
        if (context->makeCurrent(this)) {
            return true;
        }
        used = 0;
    }
    return false;
}

void ContextPoolContext::release()
{
    used = 0;
    context->makeCurrent(NULL);
}

OpenGLRenderer::OpenGLRenderer()
{
}

OpenGLRenderer::~OpenGLRenderer()
{

}

void OpenGLRenderer::set_viewpoint_output(int id, OpenGLOutput* output)
{

}

void OpenGLRenderer::render_viewpoints()
{
    int left = 0;
    int right = 0;

    if (active_viewpoint.left.enabled) {
        QFuture left_render = QtConcurrent::run(render_eye());
        left = active_viewpoint.left.fbo->texture();
    }

    if (active_viewpoint.right.enabled) {
        QFuture right_render = QtConcurrent::run(render_eye());
        right = active_viewpoint.right.fbo->texture();
    }

    left_render.waitForFinished();
    right_render.waitForFinished();

    output->set_textures(left, right);
    output->submit();
}

void OpenGLRenderer::render_viewpoint(const float (&proj)[4][4], const float (&view)[4][4])
{

}
