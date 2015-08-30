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
};

layout(std140, location = 1) uniform ShaderParameters
{
    mat4 transforms[1024];
};

layout(location = 0) out vec3 vertex_position;
layout(location = 1) out vec3 vertex_normal;
layout(location = 2) out vec2 vertex_texcoord;

layout(location = 3) flat out int draw_id;

void main()
{
    draw_id = int(draw_info[2]);
    mat4 transform = transforms[int(draw_info[0])];
    gl_Position = view_projection * transform * vec4(position, 1.0);
    vertex_position = (transform * vec4(position, 1.0)).xyz;
    vertex_normal = (transform * vec4(normal, 0.0)).xyz;
    vertex_texcoord = texcoord;
}
