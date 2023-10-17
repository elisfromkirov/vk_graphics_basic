#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

void sort(inout vec4 values[9])
{
  for (int i = 0; i < 9; ++i)
  {
    for (int j = i + 1; j < 9; ++j)
    {
      for (int channel = 0; channel < 3; ++channel)
      {
        if (values[j - 1][channel] < values[j][channel])
        {
          float tmp = values[j - 1][channel];
          values[j - 1][channel] = values[j][channel];
          values[j][channel] = tmp;
        }
      }
    }
  }
}

vec4 median(inout vec4 values[9])
{
  sort(values);
  return values[5];
}

void main()
{
  vec4 neighbourColors[9] = vec4[9](
    textureOffset(colorTex, surf.texCoord, ivec2(-1, -1)),
    textureOffset(colorTex, surf.texCoord, ivec2(-1,  0)),
    textureOffset(colorTex, surf.texCoord, ivec2(-1,  1)),
    textureOffset(colorTex, surf.texCoord, ivec2( 0, -1)),
    textureOffset(colorTex, surf.texCoord, ivec2( 0,  0)),
    textureOffset(colorTex, surf.texCoord, ivec2( 0,  1)),
    textureOffset(colorTex, surf.texCoord, ivec2( 1, -1)),
    textureOffset(colorTex, surf.texCoord, ivec2( 1,  0)),
    textureOffset(colorTex, surf.texCoord, ivec2( 1,  1))
  );

  color = median(neighbourColors);
}