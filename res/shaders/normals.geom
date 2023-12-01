
layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} gsIn[];

layout (location = 0) out GsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} gsOut;

layout (push_constant) uniform PushData {
    layout(offset = 16) float length;
};


#include "shaderBridge.h"

void genLine(int index) {
    CameraData camera = globalData.cameraBuffer.camera;

    vec3 normal = gsIn[index].TBN[2];
    vec4 N = camera.projection * camera.view * vec4(normal * length, 0.0);


    gl_Position = gl_in[index].gl_Position;
    gsOut.FragPos = gsIn[index].FragPos;
    gsOut.TexCoords = gsIn[index].TexCoords;
    gsOut.TBN = gsIn[index].TBN;
    gsOut.ViewPos = gsIn[index].ViewPos;
    gsOut.drawID = gsIn[index].drawID;
    EmitVertex();

    gl_Position = gl_in[index].gl_Position + N;
    gsOut.FragPos = gsIn[index].FragPos;
    gsOut.TexCoords = gsIn[index].TexCoords;
    gsOut.TBN = gsIn[index].TBN;
    gsOut.ViewPos = gsIn[index].ViewPos;
    gsOut.drawID = gsIn[index].drawID;
    EmitVertex();
    EndPrimitive();
}

void main() {
    for (int i = 0; i < gl_in.length(); i++) {
        genLine(i);
    }
}