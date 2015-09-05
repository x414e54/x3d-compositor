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
float bloom_saturation = 0;
float bloom_exponent = 0;
float bloom_scale = 0;
float tonemap_rate = 0;

//https://github.com/Jam3/glsl-fast-gaussian-blur/blob/master/13.glsl
vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  color += texture2D(image, uv) * 0.1964825501511404;
  color += texture2D(image, uv + (off1 / resolution)) * 0.2969069646728344;
  color += texture2D(image, uv - (off1 / resolution)) * 0.2969069646728344;
  color += texture2D(image, uv + (off2 / resolution)) * 0.09447039785044732;
  color += texture2D(image, uv - (off2 / resolution)) * 0.09447039785044732;
  color += texture2D(image, uv + (off3 / resolution)) * 0.010381362401148057;
  color += texture2D(image, uv - (off3 / resolution)) * 0.010381362401148057;
  return color;
}

void main()
{
    X3DEffect effect = effects[draw_id];

    vec2 texcoord = gl_FragCoord.xy / vec2(width, height);
    // TODO turn off write for other buffers
    vec3 color = texture(in_rt0, texcoord).rgb;
    rt0 = vec4(color, 1.0);
    rt1 = blur13(in_rt1, texcoord, vec2(width, height), effect.direction.xy);
}

