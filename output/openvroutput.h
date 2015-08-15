#ifndef OPENVROUTPUT_H
#define OPENVROUTPUT_H

#include "opengl/opengloutput.h"

//#include <openvr.h>

class OpenVROutput : public OpenGLOutput
{
public:
    OpenVROutput();
    virtual ~OpenVROutput();
    virtual void submit();
private:
    //vr::IVRSystem *HMD;
    //vr::IVRCompositor *compositor;
};

#endif // OPENVROUTPUT_H
