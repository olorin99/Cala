#version 450

//layout (location = 0) in vec3 inPosition;
//layout (location = 1) in vec3 inNormal;
//layout (location = 2) in vec2 inTexCoords;
//layout (location = 3) in vec3 inTangent;
//layout (location = 4) in vec3 inBitangent;

layout (location = 0) out VsOut {
    vec2 TexCoords;
} vsOut;

void main() {
//    vsOut.TexCoords = inTexCoords;
    vsOut.TexCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

//    gl_Position = vec4(inPosition, 1.0);
    gl_Position = vec4(vsOut.TexCoords * 2.0f + -1.0f, 0.0f, 1.0);
}