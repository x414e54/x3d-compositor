#ifndef OPENGLOUTPUT_H
#define OPENGLOUTPUT_H

class Output
{
    virtual void swapBuffers() = 0;
};

class QOpenGLFramebufferObject;
class QAbstractOpenGLFunctions;

class OpenGLOutput : public Output
{
public:
    OpenGLOutput();
    virtual ~OpenGLOutput();
    virtual void submit();
    virtual void set_textures(int left, int right);
    virtual void set_renderbuffers(int depthleft, int depthright);
    bool is_quad_buffered()
    {
        return quad_buffered;
    }

protected:
    bool quad_buffered;
    QOpenGLFramebufferObject* left;
    QOpenGLFramebufferObject* right;
    QAbstractOpenGLFunctions* gl;
};

#endif // OPENGLOUTPUT_H
