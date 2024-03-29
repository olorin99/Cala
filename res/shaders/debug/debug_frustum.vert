
layout (location = 0) out VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} vsOut;

#include "shaderBridge.h"

const uint cubeIndices[] = {
    0, 1, 2, //left
    2, 1, 3,
    0, 5, 1, //top
    0, 4, 5,
    5, 7, 5, //right
    4, 6, 7,
    2, 7, 3, //bottom
    2, 6, 7,
    0, 2, 4, //near
    2, 6, 4,
    7, 5, 1, //far
    1, 3, 7
};

void main() {
    vsOut.FragPos = vec3(0.0);
    vsOut.TexCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vsOut.TBN = mat3(1.0);
    vsOut.ViewPos = vec3(0.0);
    vsOut.drawID = 0;

    uint index = cubeIndices[gl_VertexIndex];
    GPUCamera cullingCamera = globalData.cameraBuffer.camera;
    vec4 vertex = cullingCamera.frustum.corners[index];

    GPUCamera mainCamera = globalData.cameraBuffer[globalData.primaryCameraIndex].camera;
    gl_Position = mainCamera.projection * mainCamera.view * vertex;
}