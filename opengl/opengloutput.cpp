#include "opengloutput.h"

#include <QtGui/QOpenGLFunctions_3_2_Core>

OpenGLOutput::OpenGLOutput() : left(0), right(0), gl(nullptr)
{
}

OpenGLOutput::~OpenGLOutput()
{

}

void OpenGLOutput::submit()
{
    ScopedOutputContext context(*this);
    if (left != 0) {
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, left);
        gl->glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (is_quad_buffered()) {
            gl->glDrawBuffer(GL_BACK_LEFT);
            gl->glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            gl->glDrawBuffer(GL_BACK_RIGHT);
            if (right != 0) {
                gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, right);
                gl->glReadBuffer(GL_COLOR_ATTACHMENT0);
                gl->glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            } else {
                gl->glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
        } else {
            gl->glDrawBuffer(GL_BACK);
            gl->glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    } else {
        gl->glClear(GL_COLOR_BUFFER_BIT);
    }
}

void OpenGLOutput::set_renderbuffers(int depthleft, int depthright)
{

}

void OpenGLOutput::set_textures(int left, int right)
{
    ScopedOutputContext context(*this);
    if (this->left != 0) {
        gl->glDeleteFramebuffers(1, &this->left);
    }

    if (left != 0) {
        gl->glGenFramebuffers(1, &this->left);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, this->left);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, left, 0);
    }

    if (this->right != 0) {
        gl->glDeleteFramebuffers(1, &this->right);
    }

    if (right != 0) {
        gl->glGenFramebuffers(1, &this->right);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, this->right);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, right, 0);
    }
    gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}
