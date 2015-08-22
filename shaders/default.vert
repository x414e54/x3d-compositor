#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require

layout(location = 0) in ivec4 draw_info;
layout(location = 1) in vec3 position;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texcoord;

layout(std140, location = 0) uniform GlobalParameters
{
    mat4 view;
    mat4 projection;
    mat4 view_projection;
};

layout(std140, location = 1) uniform ShaderParameters
{
    mat4 transforms[1024];
};

layout(location = 0) out _vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoord;
} vertex;

layout(location = 1) out _object
{
    int draw_id;
} object;

void main()
{
    int draw_id = draw_info[0];
    mat4 transform = transforms[draw_id];
    gl_Position = view_projection * transform * vec4(position, 1.0);
    vertex.position = (transform * vec4(position, 1.0)).xyz;
    vertex.normal = (transform * vec4(normal, 1.0)).xyz;
    vertex.texcoord = texcoord;
    object.draw_id = draw_id;
}
