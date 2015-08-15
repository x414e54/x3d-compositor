#include "openvroutput.h"

#include <QOpenGLContext>

OpenVROutput::OpenVROutput()
{
    /*vr::HmdError error = vr::HmdError_None;
    compositor = (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error);
    */
}


OpenVROutput::~OpenVROutput()
{

}

void OpenVROutput::submit()
{
    /*if (compositor != NULL)
    {
        compositor->Submit(vr::Eye_Left, vr::API_OpenGL, (void*)left->texture(), NULL);
        compositor->Submit(vr::Eye_Right, vr::API_OpenGL, (void*)right->texture(), NULL);
    }*/
}
