#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec4 draw_info;
layout(location = 1) in vec3 position;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(std140, binding = 0) uniform GlobalParameters
{
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    int width;
    int height;
};

layout(std140, binding = 1) uniform ObjectParameters
{
    mat4 transforms[256];
};

layout(location = 0) out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
};

layout(location = 1) flat out int draw_id;

void main()
{
    draw_id = int(draw_info[0]);
    mat4 transform = transforms[draw_id];
    if (int(draw_info[3]) == 1) {
        gl_Position = vec4(position, 1.0);
    } else {
        gl_Position = view_projection * transform * vec4(position, 1.0);
    }
}
