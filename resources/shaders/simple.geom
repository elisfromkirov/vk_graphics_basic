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

void emit_source_triangle()
{
    for (uint i = 0; i < 3; ++i)
    {
        gOut.wPos     = vOut[i].wPos;
        gOut.wNorm    = vOut[i].wNorm;
        gOut.wTangent = vOut[i].wTangent;
        gOut.texCoord = vOut[i].texCoord;

        gl_Position = gl_in[i].gl_Position;

        EmitVertex();
    }

    EndPrimitive();
}

void emit_tetrahedron()
{
    vec3 normal = cross(vOut[2].wPos - vOut[1].wPos, vOut[0].wPos - vOut[1].wPos);
    normal = normal / length(normal) * 0.02;

    vec3 median_intersection_point = vec3(
        (vOut[0].wPos.x + vOut[1].wPos.x + vOut[2].wPos.x) / 3.0,
        (vOut[0].wPos.y + vOut[1].wPos.y + vOut[2].wPos.y) / 3.0,
        (vOut[0].wPos.z + vOut[1].wPos.z + vOut[2].wPos.z) / 3.0
    );

    vec4 position = params.mProjView * vec4(median_intersection_point + normal, 1.0); 

    gl_Position = position;
    EmitVertex();
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    EndPrimitive();

    gl_Position = position;
    EmitVertex();
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    EndPrimitive();

    gl_Position = position;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();
    EndPrimitive();


    gl_Position = position;
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    EndPrimitive();
}

void main()
{
    emit_source_triangle();
    emit_tetrahedron();
}