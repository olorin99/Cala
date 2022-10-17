#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec3 inBitangent;

struct CameraData {
    mat4 projection;
    mat4 view;
//    vec3 position;
};

//layout (set = 0, binding = 0) uniform FrameData {
//    CameraData camera;
//};

layout (push_constant) uniform PushConstants {
    CameraData camera;
};

layout (set = 1, binding = 0) uniform ModelData {
    mat4 model;
};

void main() {
    gl_Position = camera.projection * camera.view * model * vec4(inPosition, 1.0);
}