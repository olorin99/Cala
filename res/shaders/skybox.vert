#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec3 inTangent;

layout (location = 0) out VsOut {
    vec3 TexCoords;
} vsOut;

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
};

layout (set = 1, binding = 0) uniform FrameData {
    CameraData camera;
};

void main() {
    vsOut.TexCoords = inPosition;

    gl_Position = (camera.projection * mat4(mat3(camera.view)) * vec4(inPosition, 1.0)).xyww;
}
