#ifndef OPENVROUTPUT_H
#define OPENVROUTPUT_H

#include "opengl/opengloutput.h"

#define COMPILER_GCC
#include <openvr.h>

class OpenVROutput : public OpenGLOutput
{
public:
    OpenVROutput();
    virtual ~OpenVROutput();
    virtual void submit();
    virtual void get_eye_projection_matrix(glm::mat4x4 &left, glm::mat4x4 &right, float near, float far);
    virtual void get_eye_matrix(glm::mat4x4 &left, glm::mat4x4 &right);
    virtual void set_textures(int left, int right, size_t width, size_t height);
private:
    void update_poses();
    vr::IVRSystem *hmd;
    vr::IVRCompositor *compositor;
};

#endif // OPENVROUTPUT_H
