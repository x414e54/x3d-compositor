#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require
#extension GL_ARB_shading_language_420pack: require

layout(std140, location = 0) uniform GlobalParameters
{
    mat4 view;
    mat4 projection;
    mat4 view_projection;
};

struct X3DMaterialNode
{
    float ambient_intensity;
    vec4 diffuse_color;
    vec3 emissive_color;
    float shininess;
    vec3 specular_color;
};

layout(std140, location = 1) uniform ShaderParameters
{
    X3DMaterialNode material[1024];
};
 
layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec2 vertex_texcoord; 

layout(location = 3) flat in int draw_id;

layout(location = 0) out vec4 rt0;
layout(location = 1) out vec4 rt1;
layout(location = 2) out vec4 rt2;
layout(location = 3) out vec4 rt3;

void main()
{
    // Be wasteful for now
    rt0 = vec4(vertex_position, specular_color.r);
    rt1 = vec4(normalize(vertex_normal), specular_color.g);
    rt2 = material[draw_id].diffuse_color;
    rt3 = vec4(material[draw_id].emissive_color, specular_color.b);
}

