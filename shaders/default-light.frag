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

uniform sampler2D in_rt0;
uniform sampler2D in_rt1;
uniform sampler2D in_rt2;
uniform sampler2D in_rt3;

layout(location = 0) in _vertex
{
    flat vec4 draw_info;
    vec3 position;
    vec3 normal;
    vec2 texcoord;
} vertex;

layout(location = 0) out vec4 rt0;

void main()
{
    int draw_id = int(vertex.draw_info[0]);
    // Be wasteful for now
    rt0.rgb = vertex.position;
}

