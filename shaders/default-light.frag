#version 150 
#extension GL_ARB_separate_shader_objects: require
#extension GL_ARB_explicit_attrib_location: require
#extension GL_ARB_explicit_uniform_location: require
#extension GL_ARB_shading_language_420pack: require

struct X3DLightNode
{
    int type;
    float intensity;
    vec4 color;
    vec3 attenuation;
    float ambient_intensity;
    vec3 position;
};

layout(std140, location = 0) uniform GlobalParameters
{
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec3 position;
    int width;
    int height;
};

layout(std140, location = 1) uniform ShaderParameters
{
    X3DLightNode lights[1024];
};

layout(binding = 0) uniform sampler2D in_rt0;
layout(binding = 1) uniform sampler2D in_rt1;
layout(binding = 2) uniform sampler2D in_rt2;
layout(binding = 3) uniform sampler2D in_rt3;

layout(location = 0) flat in int draw_id;

layout(location = 0) out vec4 rt0;

// Calculations from X3D spec at 
// http://www.web3d.org/documents/specifications/19775-2/V3.0/Part01/components/lighting.html#t-spotlightfactor

vec3 x3d_light_ambient(float light_ambient_intensity, vec3 mat_color,
                       float mat_ambient_intensity)
{
    return light_ambient_intensity * mat_color * mat_ambient_intensity;
}

vec3 x3d_light_diffuse(float intensity, vec3 mat_color, vec3 normal,
                       vec3 light_normal)
{
    return intensity * mat_color * dot(normal, light_normal);
}

vec3 x3d_light_specular(float intensity, float shininess, vec3 spec_color,
                        vec3 normal, vec3 light_normal, vec3 eye_normal)
{
    vec3 lv = light_normal + eye_normal;
    return intensity * spec_color * pow(dot(normal, lv / length(lv)),
                                        shininess * 128);
}

float x3d_light_attenuation(float d, vec3 a)
{
    return 1.0 / max(a[0] + (a[1] * d) + (a[2] * d * d), 1.0);
}

vec4 x3d_light(vec3 mat_emissive, float attenuation, float spoti,
               vec3 light_color, vec3 ambient, vec3 diffuse, vec3 specular)
{
    // TODO X3D fog. IFrgb × (1 -f0) + f0 × x3d_light
    return mat_emissive + ((attenuation * spoti * light_color)
                           * (ambient + diffuse + specular));
}

void main()
{
    X3DLightNode light = lights[0];//raw_id];

    vec2 texcoord = gl_FragCoord.xy / vec2(width, height);
    vec4 pos = texture(in_rt0, texcoord);
    vec4 norm = texture(in_rt1, texcoord);
    vec4 color = texture(in_rt2, texcoord);
    float ambient_intensity = color.a;
    float shininess = 0.0;
    vec4 emissive = texture(in_rt3, texcoord);
    vec3 specular_color = vec3(pos.a, norm.a, emissive.a);

    vec3 light_direction = pos.xyz - light.position;
    float distance = length(light_direction);
    vec3 light_normal = normalize(light_direction);
    vec3 eye_normal = normalize(position);

    if (light.type == 1) {
        // TODO check this?
        light_normal = -light_direction;
    }
    
    float attenuation = x3d_light_attenuation(distance, light.attenuation);
    vec3 ambient = x3d_light_ambient(light.ambient_intensity, color.rgb, ambient_intensity);
    vec3 diffuse = x3d_light_diffuse(light.intensity, color.rgb, norm.xyz, light_normal);
    vec3 specular = x3d_light_specular(light.intensity, shininess, specular_color, norm.xyz, light_normal, eye_normal);

    float spoti = 1;
    if (light.type == 2) {
        // TODO calculate spoti;
    }
    rt0 = x3d_light(emissive.rgb, attenuation, spoti, light.color.rgb, ambient, diffuse, specular);
}

