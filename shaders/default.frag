#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require
#extension GL_ARB_shading_language_420pack: require
#extension GL_NV_bindless_texture : enable
#extension GL_NV_gpu_shader5 : enable

layout(std140, binding = 0) uniform GlobalParameters
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
    ivec4 ambient_offset_width_height;
    ivec4 diffuse_offset_width_height;
    ivec4 specular_offset_width_height;
    ivec4 normal_offset_width_height;
    ivec4 displacement_offset_width_height;
    ivec4 alpha_offset_width_height;
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

layout(std140, binding = 3) uniform ShaderParameters
{
    X3DAppearanceNode apperances[256];
};

#ifdef GL_NV_bindless_texture
layout(std140, binding = 4) uniform TextureParameters
{
    sampler2D textures[256];
};
#else
layout(binding = 0) uniform samplerBuffer textures;
#endif

layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec2 vertex_texcoord; 

layout(location = 4) flat in int draw_id;

layout(location = 0) out vec4 rt0;
layout(location = 1) out vec4 rt1;
layout(location = 2) out vec4 rt2;
layout(location = 3) out vec4 rt3;
layout(location = 4) out vec4 rt4;
layout(location = 5) out vec4 rt5;

#ifdef GL_NV_bindless_texture
vec4 get_texel(ivec4 o_w_h, vec2 tex_coord)
{
    vec4 texel = texture(textures[o_w_h.x], tex_coord);
    if (o_w_h[1] == 0 || o_w_h[2] == 0) {
        return vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        return texel;
    }
}
#else
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
#endif

void main()
{
    // Be wasteful for now
    X3DMaterialNode material = apperances[draw_id].material;
    vec4 ambient_texel = get_texel(apperances[draw_id].texture.ambient_offset_width_height, vertex_texcoord);
    vec4 diffuse_texel = get_texel(apperances[draw_id].texture.diffuse_offset_width_height, vertex_texcoord);
    vec4 alpha_texel = get_texel(apperances[draw_id].texture.alpha_offset_width_height, vertex_texcoord);
    vec4 normal_texel = get_texel(apperances[draw_id].texture.normal_offset_width_height, vertex_texcoord);
    vec4 specular_texel = get_texel(apperances[draw_id].texture.specular_offset_width_height, vertex_texcoord);
    //vec4 emissive_texel = get_texel(apperances[draw_id].texture.emissive_offset_width_height, vertex_texcoord);

    float alpha = alpha_texel.a * diffuse_texel.a * material.diffuse_color.a;
    if (alpha == 0.0) {
        discard;
    }

    rt0 = vec4(vertex_position, 1.0);
    rt1 = vec4(normalize(vertex_normal), 1.0);
    rt2 = vec4(diffuse_texel.rgb, alpha_texel.r * diffuse_texel.a * material.diffuse_color.a);
    rt3 = vec4(ambient_texel.rgb, material.emissive_ambient_intensity.a);
    rt4 = vec4(specular_texel.rgb + material.specular_shininess.rgb, material.specular_shininess.a);
    rt5 = vec4(material.emissive_ambient_intensity.rgb, 1.0);
}

