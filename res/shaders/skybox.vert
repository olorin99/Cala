
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out VsOut {
    vec3 TexCoords;
} vsOut;

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    float exposure;
};

layout (set = 0, binding = 1) buffer CameraBuffer { CameraData camera; } globalBuffersCamera[];

struct GlobalData {
    float gamma;
    uint time;
    int meshBufferIndex;
    int materialBufferIndex;
    int lightBufferIndex;
    int cameraBufferIndex;
};

layout (set = 1, binding = 0) uniform Global {
    GlobalData globalData;
};

void main() {
    vsOut.TexCoords = inPosition;

    gl_Position = (globalBuffersCamera[globalData.cameraBufferIndex].camera.projection * mat4(mat3(globalBuffersCamera[globalData.cameraBufferIndex].camera.view)) * vec4(inPosition, 1.0)).xyww;
}
