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

layout (location = 0) in vec4 in_Position;
layout (location = 1) in vec4 in_Normal;

layout (location = 0) out vec4 out_Position;
layout (location = 1) out vec4 out_Normal;
layout (location = 2) out vec4 out_Flux;

void main()
{
	out_Position = in_Position;
	out_Normal = in_Normal;
    out_Flux = Params.Intensity * vec4(Params.LightColor * Color, 1.0f);
}
