#ifndef OPENGLOUTPUT_H
#define OPENGLOUTPUT_H

#include <cstddef>

class Output
{
public:
    Output() : output_width(0), output_height(0),
               stereo(false), enabled(false) {}
    size_t output_width;
    size_t output_height;
    bool stereo;
    bool enabled;
    bool is_stereo() { return stereo; }
    bool is_enabled() { return enabled; }
    virtual void swap_buffers() {}
};

class QOpenGLFunctions_3_2_Core;
class QOpenGLContext;

class OpenGLOutput : public Output
{
public:
    OpenGLOutput();
    virtual ~OpenGLOutput();
    virtual void submit();
    virtual void set_textures(int left, int right, size_t width, size_t height);
    virtual void set_depth_textures(int depthleft, int depthright);
    bool is_quad_buffered()
    {
        return quad_buffered;
    }

    virtual void init_context(QOpenGLContext*) = 0;
    virtual void make_current() = 0;
    virtual void done_current() = 0;
protected:
    bool quad_buffered;
    unsigned int left;
    unsigned int right;
    size_t fbo_width;
    size_t fbo_height;
    QOpenGLFunctions_3_2_Core* gl;
};

class ScopedOutputContext
{
public:
    ScopedOutputContext(OpenGLOutput& output) : output(output) {
        output.make_current();
    }
    virtual ~ScopedOutputContext() {
        output.done_current();
    }
    OpenGLOutput& output;
};

#endif // OPENGLOUTPUT_H
