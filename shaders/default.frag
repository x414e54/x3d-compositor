#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require

struct X3DMaterialNode
{
    float ambient_intensity;
    vec4 diffuse_color;
    vec3 emissive_color;
    float shininess;
    vec3 specular_color;
};

layout(std140, location = 0) uniform ShaderParameters
{
    X3DMaterialNode material[1024];
};

//sampler2D texture;

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
layout(location = 1) out vec4 rt1;
layout(location = 2) out vec4 rt2;

void main()
{
    // Be wasteful for now
    rt0.rgb = vertex.position;
    rt1.rgb = vertex.normal;
    rt2.rgb = material[object.draw_id].diffuse_color.rgb;
}

