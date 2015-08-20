#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 projection;
layout(location = 2) uniform mat4 view_projection;
layout(location = 3) uniform mat4 model;

layout(location = 0) out _vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoord;
} vertex;

void main()
{
    gl_Position.xyz = position; //view_projection * model * vec4(position, 1.0);
    vertex.position = (view * model * vec4(position, 1.0)).rgb;
    vertex.normal = normal;
    vertex.texcoord = texcoord;
}
