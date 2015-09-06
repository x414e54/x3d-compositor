#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require
#extension GL_ARB_shading_language_420pack: require

struct X3DEffect
{
    int type;
    vec4 direction;
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
layout(location = 1) out vec4 rt1;

float average_luminance = 0;
float min_exposure = -1.1;
float max_exposure = 1.1;

float luminance(vec3 color)
{
    return max(dot(color, vec3(0.299, 0.587, 0.114)), 0.00001);
}

vec3 expose(vec3 color, float threshold)
{
    float exposure = 0;
    average_luminance = max(average_luminance, 0.00001);
	float key_value = 1.03 - (2.0 / (2.0 + log(average_luminance + 1.0)));
	float linear_exposure = (key_value / average_luminance);
//	exposure = clamp(log2(max(linear_exposure, 0.00001)), min_exposure, max_exposure);
	exposure = log2(max(linear_exposure, 0.00001));
    exposure -= threshold;
    return exp2(exposure) * color;
}

float get_average_luminance(vec2 texcoord)
{
    return exp(texture(in_rt5, texcoord, 10).a);
}

void main()
{
    X3DEffect effect = effects[draw_id];

    vec2 texcoord = gl_FragCoord.xy / vec2(width, height);
    average_luminance = get_average_luminance(texcoord);
    vec4 color = texture(in_rt5, texcoord);

    rt0 = vec4(expose(color.rgb, 0.0), 1.0);
    vec3 brightpass = expose(color.rgb, 3.0);
    if (luminance(color.rgb) > 0.001) {
        rt1 = vec4(brightpass, 1.0);
    } else {
        rt1 = vec4(0.0, 0.0, 0.0, 1.0);
    }

    if (render_type == 0) {
    } else if (render_type > 0 || render_type == -1) {
        rt0 = vec4(color.rgb, 1.0);
    } else if (render_type == -2) {
        rt0 = vec4(vec3(exp(color.a)), 1.0);
    } else if (render_type == -3) {
        rt0 = vec4(vec3(average_luminance), 1.0);
    } else if (render_type == -4) {
        rt0 = rt1;
    }
}

