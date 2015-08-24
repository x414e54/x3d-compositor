#include "openvroutput.h"

#define COMPILER_GCC
#include <openvr.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

OpenVROutput::OpenVROutput()
{
    vr::HmdError error = vr::HmdError_None;
    compositor = (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error);
    hmd = vr::VR_Init(&error);
}


OpenVROutput::~OpenVROutput()
{

}

void OpenVROutput::update_poses()
{
    if (hmd == nullptr || compositor == nullptr) {
        return;
    }

    vr::TrackedDevicePose_t device_pose[vr::k_unMaxTrackedDeviceCount];

    compositor->WaitGetPoses(device_pose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

    for (int device = 0; device < vr::k_unMaxTrackedDeviceCount; ++device)
    {
        if (device_pose[device].bPoseIsValid)
        {
            glm::mat4x4 pose = glm::mat4x4(glm::make_mat3x4(&device_pose[device].mDeviceToAbsoluteTracking.m[0][0]));

            switch (hmd->GetTrackedDeviceClass(device))
            {
                case vr::TrackedDeviceClass_Controller:
                    // post controller event
                    break;
                case vr::TrackedDeviceClass_HMD:
                    // post view/movement event
                    break;
                default:
                    break;
            }
        }
    }
}

void OpenVROutput::submit()
{
    if (compositor != nullptr) {
        compositor->Submit(vr::Eye_Left, vr::API_OpenGL, (void*)left, nullptr);
        compositor->Submit(vr::Eye_Right, vr::API_OpenGL, (void*)right, nullptr);
    }

    update_poses();
}

void OpenVROutput::set_textures(int left, int right, size_t, size_t)
{
    this->left = left;
    this->right = right;
}

void OpenVROutput::get_eye_projection_matrix(glm::mat4x4 &left, glm::mat4x4 &right, float near, float far)
{
    if (hmd == nullptr) {
        return;
    }

    vr::HmdMatrix44_t proj = hmd->GetProjectionMatrix(vr::Eye_Left, near, far, vr::API_OpenGL);
    left = glm::make_mat4x4(&proj.m[0][0]);

    proj = hmd->GetProjectionMatrix(vr::Eye_Right, near, far, vr::API_OpenGL);
    right = glm::make_mat4x4(&proj.m[0][0]);
}

void OpenVROutput::get_eye_matrix(glm::mat4x4 &left, glm::mat4x4 &right)
{
    if (hmd == nullptr) {
        return;
    }

    vr::HmdMatrix34_t eye = hmd->GetEyeToHeadTransform(vr::Eye_Left);
    left = glm::inverse(glm::mat4x4(glm::make_mat3x4(&eye.m[0][0])));

    eye = hmd->GetEyeToHeadTransform(vr::Eye_Right);
    right = glm::inverse(glm::mat4x4(glm::make_mat3x4(&eye.m[0][0])));
}
