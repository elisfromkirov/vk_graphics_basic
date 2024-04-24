#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (binding = 1) uniform sampler2D ColorMap;

layout (location = 0) in vec2 TexCoord;

layout (location = 0) out vec4 Color;

void main()
{
	const vec3 color = textureLod(ColorMap, TexCoord, 0).rgb;

	switch (Params.ToneMappingMode)
	{
	case NO_TONE_MAPPING:
		Color = vec4(color, 1.0f);
		break;
	case EXPOSURE_TONE_MAPPING:
		Color = vec4(pow(vec3(1.0f) - exp(-color * Params.Exposure), vec3(1.0f / Params.Gamma)), 1.0f);
		break;
	case REINHARD_TONE_MAPPING:
		Color = vec4(pow(color / (color + vec3(1.0f)), vec3(1.0f / Params.Gamma)), 1.0f);
		break;
	}
}
