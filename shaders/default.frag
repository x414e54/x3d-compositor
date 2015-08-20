#version 150
#extension ARB_separate_shader_objects : require

struct X3DMaterialNode
{
    float ambientIntensity;
    vec4 diffuseColor;
    vec3 emissiveColor;
    float shininess;
    vec3 specularColor;
};

layout(std140, location = 0) uniform ShaderParameters
{
    X3DMaterialNode material;
};

//sampler2D texture;

layout(location = 0) in vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoord;
};

layout(location = 0) out position;
layout(location = 1) out normal;
layout(location = 2) out diffuse;

int main()
{
    // Be wasteful for now
    gl_FragData[0].rgb = vertex.position;
    gl_FragData[1].rgb = vertex.normal;
    gl_FragData[2].rgb = diffuseColor;
}

