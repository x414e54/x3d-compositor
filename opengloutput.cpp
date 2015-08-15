#include "opengloutput.h"

#include <QOpenGLFramebufferObject>

OpenGLOutput::OpenGLOutput()
{
}

OpenGLOutput::~OpenGLOutput()
{

}

void OpenGLOutput::submit()
{
    if (left != NULL) {
        if (is_quad_buffered()) {
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glDrawBuffer(GL_BACK_LEFT);
            QOpenGLFramebufferObject::blitFramebuffer(0, left);
            glDrawBuffer(GL_BACK_RIGHT);
            if (right != NULL) {
                QOpenGLFramebufferObject::blitFramebuffer(0, right);
            } else {
                QOpenGLFramebufferObject::blitFramebuffer(0, left);
            }
        } else {
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glDrawBuffer(GL_BACK);
            QOpenGLFramebufferObject::blitFramebuffer(0, left);
        }
    }
}

void OpenGLOutput::set_textures(int left, int right)
{
    QOpenGLFramebufferObjectFormat format;
    format.setSamples(0);
    format.setMipmap(false);
    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);

    if (this->left != NULL) {
        delete this->left;
    }

    if (left != 0) {
        format.setTextureTarget(left);
        this->left = new QOpenGLFramebufferObject(0, 0, format);
    }

    if (this->right != NULL) {
        delete this->right;
    }

    if (right != 0) {
        format.setTextureTarget(right);
        this->right = new QOpenGLFramebufferObject(0, 0, format);
    }
}
