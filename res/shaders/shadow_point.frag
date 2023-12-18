
layout (location = 0) in VsOut {
    vec3 FragPos;
} fsIn;

#include "shaderBridge.h"

layout (set = 3, binding = 0) readonly buffer LightData {
    GPULight light;
};

struct ShadowCameraData {
    mat4 viewProjection;
    vec3 position;
    float near;
    float far;
};

layout (push_constant) uniform PushConstants {
    ShadowCameraData camera;
};

void main() {
    vec3 lightVec = fsIn.FragPos - light.position.xyz;
    gl_FragDepth = length(lightVec) / (camera.far - camera.near);
}