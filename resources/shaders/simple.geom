#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 12) out;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout(location = 0) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut[];

layout(location = 0) out GS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} gOut;

void EmitSourceVertex(uint i)
{
    gOut.wPos     = vOut[i].wPos;
    gOut.wNorm    = vOut[i].wNorm;
    gOut.wTangent = vOut[i].wTangent;
    gOut.texCoord = vOut[i].texCoord;

    gl_Position = gl_in[i].gl_Position;

    EmitVertex();
}

void EmitTetrahedronVertex()
{
    vec3 normal = normalize(cross(vOut[2].wPos - vOut[1].wPos, vOut[0].wPos - vOut[1].wPos)) * 0.02;

    gOut.wPos     = (vOut[0].wPos + vOut[1].wPos + vOut[2].wPos) / 3.0 + normal;
    gOut.wNorm    = normal;
    gOut.wTangent = (vOut[0].wTangent + vOut[1].wTangent + vOut[2].wTangent) / 3.0;
    gOut.texCoord = (vOut[0].texCoord + vOut[1].texCoord + vOut[2].texCoord) / 3.0;

    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);

    EmitVertex();
}

void EmitSourceTriangle()
{
    for (uint i = 0; i < 3; ++i)
    {
        EmitSourceVertex(i);
    }
}

void EmitTetrahedron()
{
    EmitTetrahedronVertex();
    EmitSourceVertex(0);
    EmitSourceVertex(1);
}

void main()
{
    EmitSourceTriangle();
    EmitTetrahedron();
    EndPrimitive();
}