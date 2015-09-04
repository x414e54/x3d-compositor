#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require
#extension GL_ARB_shading_language_420pack: require

struct X3DEffect
{
    int type;
    int direction;
    float bloom_saturation;
    float bloom_exponent;
    float bloom_scale;
    float tonemap_rate;
};

layout(std140, binding = 0) uniform GlobalParameters
{
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec4 position;
    int width;
    int height;
    int render_type;
};

layout(std140, binding = 3) uniform ShaderParameters
{
    X3DEffect effects[256];
};

layout(binding = 1) uniform sampler2D in_rt0;
layout(binding = 2) uniform sampler2D in_rt1;
layout(binding = 3) uniform sampler2D in_rt2;
layout(binding = 4) uniform sampler2D in_rt3;
layout(binding = 5) uniform sampler2D in_rt4;
layout(binding = 6) uniform sampler2D in_rt5;

layout(location = 1) flat in int draw_id;

layout(location = 0) out vec4 rt0;

vec3 tonemap_filmic(vec3 color)
{
    color = max(vec3(color - 0.004), 0.0);
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7)+ 0.06);
    return color;
}

void main()
{
    X3DEffect effect = effects[draw_id];

    vec2 texcoord = gl_FragCoord.xy / vec2(width, height);
    vec3 color = texture(in_rt0, texcoord).rgb;
    vec3 bloom = texture(in_rt1, texcoord).rgb;

    if (render_type == 0) {
        rt0 = vec4(tonemap_filmic(color + bloom), 1.0);
    } else {
        rt0 = vec4(color, 1.0);
    }
}

