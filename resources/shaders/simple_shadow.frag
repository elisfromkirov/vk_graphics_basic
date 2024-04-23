#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

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

layout (binding = 1) uniform sampler2D DepthMap;

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

    out_Color = vec4(DirectLightingColor, 1.0f);
}
