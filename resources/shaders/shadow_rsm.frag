#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "samples.h"

layout (push_constant) uniform PushConstant
{
    mat4 ProjView;
    mat4 Model;
    vec3 Color;
};

layout (binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (binding = 1, set = 0) uniform sampler2D DepthMap;
layout (binding = 2, set = 0) uniform sampler2D PositionMap;
layout (binding = 3, set = 0) uniform sampler2D NormalMap;
layout (binding = 4, set = 0) uniform sampler2D FluxMap;

layout (location = 0) in vec4 in_Position;
layout (location = 1) in vec4 in_Normal;

layout (location = 0) out vec4 out_Color;

void main()
{
    const vec4 LightClipSpacePosition = Params.LightProjView * in_Position;
    const vec4 LightNDCPosition = LightClipSpacePosition / LightClipSpacePosition.w;
    const vec2 TexCoord = LightNDCPosition.xy * 0.5f + vec2(0.5f, 0.5f);

    vec3 DirectLightingColor = vec3(0.0f, 0.0f, 0.0f);
    if (TexCoord.x > 0.0001f && TexCoord.x < 0.9999f && TexCoord.y > 0.0001f && TexCoord.y < 0.9999f)
    {
       if (LightNDCPosition.z < textureLod(DepthMap, TexCoord, 0).x + 0.001f)
       {
           DirectLightingColor = max(dot(in_Normal, Params.LightPosition - in_Position), 0.0f) * Params.LightColor * Color;
       }
    }

    vec3 IndirectLightingColor = vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        const vec2 SampleTexCoord = TexCoord + SAMPLES[i] * Params.Radius;

        const vec4 SamplePosition = textureLod(PositionMap, SampleTexCoord, 0);

        const vec4 SampleNormal = textureLod(NormalMap, SampleTexCoord, 0);

        const vec3 SampleFlux = textureLod(FluxMap, SampleTexCoord, 0).xyz;

        IndirectLightingColor += SampleFlux
                               * max(dot(SampleNormal, in_Position - SamplePosition), 0.0f)
                               * max(dot(in_Normal, SamplePosition - in_Position), 0.0f)
                               * pow(SAMPLES[i].x, 2)
                               / pow(length(in_Position - SamplePosition), 4);
    }

    out_Color = vec4(DirectLightingColor + IndirectLightingColor, 1.0f);
}
