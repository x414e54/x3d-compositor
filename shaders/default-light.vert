#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require

layout(location = 0) in vec4 draw_info;
layout(location = 1) in vec3 position;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(std140, location = 0) uniform GlobalParameters
{
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    int width;
    int height;
};

struct X3DLightNode
{
    mat4 transform;
    int type;
};

layout(std140, location = 2) uniform ShaderParameters
{
    X3DLightNode lights[1024];
};

layout(location = 0) flat out int draw_id;

void main()
{
    draw_id = int(draw_info[0]);
    X3DLightNode light = lights[draw_id];
    if (light.type == 1) {
        gl_Position = vec4(position, 1.0);
    } else {
        gl_Position = view_projection * light.transform * vec4(position, 1.0);
    }
}
