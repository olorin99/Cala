
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec3 inTangent;

#include "shaderBridge.h"

layout (push_constant) uniform PushConstants {
    uint cameraIndex;
};

void main() {
    GPUCamera camera = globalData.cameraBuffer[cameraIndex].camera;
    mat4 model = globalData.transformsBuffer.transforms[gl_BaseInstance];
    gl_Position = camera.projection * camera.view * model * vec4(inPosition, 1.0);
}