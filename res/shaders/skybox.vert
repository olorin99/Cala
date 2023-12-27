
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;

layout (location = 0) out VsOut {
    vec3 TexCoords;
} vsOut;

#include "shaderBridge.h"

void main() {
    vsOut.TexCoords = inPosition;

    GPUCamera camera = globalData.cameraBuffer[globalData.primaryCameraIndex].camera;

    gl_Position = (camera.projection * mat4(mat3(camera.view)) * vec4(inPosition, 1.0)).xyww;
}
