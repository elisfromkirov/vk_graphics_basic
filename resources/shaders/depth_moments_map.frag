#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec2 oDepthMoments;

void main()
{
	oDepthMoments = vec2(gl_FragCoord.z, gl_FragCoord.z * gl_FragCoord.z);
}
