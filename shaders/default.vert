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

layout(location = 0) out _vertex
{
    flat vec4 draw_info;
    vec3 position;
    vec3 normal;
    vec2 texcoord;
} vertex;

void main()
{
    int draw_id = int(draw_info[0]);
    mat4 transform = transforms[draw_id];
    gl_Position = view_projection * transform * vec4(position, 1.0);
    vertex.position = (view * transform * vec4(position, 1.0)).rgb;
    vertex.normal = normal;
    vertex.texcoord = texcoord;
    vertex.draw_info = draw_info;
}
