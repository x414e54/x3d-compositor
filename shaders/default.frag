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

struct X3DTextureTransformNode
{
    vec4 center_scale;
    vec4 translation_rotation;
};

struct X3DTextureNode
{
    ivec4 offset_width_height;
};

struct X3DMaterialNode
{
    vec4 diffuse_color;
    vec4 emissive_ambient_intensity;
    vec4 specular_shininess;
};

struct X3DAppearanceNode
{
    X3DMaterialNode material;
    X3DTextureTransformNode tex_transform;
    X3DTextureNode texture;
};

layout(std140, location = 1) uniform ShaderParameters
{
    X3DAppearanceNode apperances[20];
};

layout(binding = 0) uniform samplerBuffer textures;

layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec2 vertex_texcoord; 

layout(location = 4) flat in int draw_id;

layout(location = 0) out vec4 rt0;
layout(location = 1) out vec4 rt1;
layout(location = 2) out vec4 rt2;
layout(location = 3) out vec4 rt3;

vec4 get_texel(ivec4 o_w_h, vec2 tex_coord)
{
    int offset = o_w_h[0] + (int(o_w_h[2] * tex_coord.y) * o_w_h[1]) + int(o_w_h[1] * tex_coord.x);
    vec4 texel = texelFetch(textures, offset);
    if (o_w_h[1] == 0 || o_w_h[2] == 0) {
        return vec4(0.0, 0.0, 0.0, 0.0);
    } else {
        return texel;
    }
}

void main()
{
    // Be wasteful for now
    X3DMaterialNode material = apperances[draw_id].material;
    vec4 texel = get_texel(apperances[draw_id].texture.offset_width_height, vertex_texcoord);
    rt0 = vec4(vertex_position, material.specular_shininess.r);
    rt1 = vec4(normalize(vertex_normal), material.specular_shininess.g);
    rt2 = vec4(material.diffuse_color.rgb + texel.rgb, material.emissive_ambient_intensity.a);
    rt3 = vec4(material.emissive_ambient_intensity.rgb, material.specular_shininess.b);
}

