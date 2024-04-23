#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout (push_constant) uniform PushConstant
{
    mat4 ProjView;
    mat4 Model;
    vec4 Color;
};

layout (location = 0) in vec4 in_PositionNormal;
layout (location = 1) in vec4 in_TexCoordTangent;

layout (location = 0) out vec4 out_Position;
layout (location = 1) out vec4 out_Normal; 

void main(void)
{
    const vec4 position = vec4(in_PositionNormal.xyz, 1.0f);
    const vec4 normal = vec4(DecodeNormal(floatBitsToInt(in_PositionNormal.w)), 0.0f);

    out_Position = Model * position;
    out_Normal = normalize(transpose(inverse(Model)) * normal);

    gl_Position = ProjView * out_Position;
}
