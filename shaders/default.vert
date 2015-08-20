#version 150
#extension ARB_separate_shader_objects : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 projection;
layout(location = 2) uniform mat4 view_projection;
layout(location = 3) uniform mat4 model;

layout(location = 0) out vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoord;
};

int main()
{
    gl_Position = view_projection * model * position;
    vertex.position = view * model * position;
    vertex.normal = normal;
    vertex.texcoord = texcoord;
}
