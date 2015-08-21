#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require

struct X3DLightNode
{
    int type;
    float intensity;
    vec4 color;
    vec4 diffuse_color;
    vec3 ambient_color;
};

layout(std140, location = 0) uniform ShaderParameters
{
    X3DLightNode light[1024];
};

layout(binding = 0) uniform sampler2D in_rt0;
layout(binding = 1) uniform sampler2D in_rt1;
layout(binding = 2) uniform sampler2D in_rt2;
layout(binding = 3) uniform sampler2D in_rt3;

layout(location = 0) in _vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoord;
} vertex;

layout(location = 1) in _object
{
    int draw_id;
} object;

layout(location = 0) out vec4 rt0;

void main()
{
    rt0 = texture(in_rt0, gl_fragCoord.xy);
}

