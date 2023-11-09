
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out VsOut {
    vec3 TexCoords;
} vsOut;

#include "camera.glsl"

#include "global_data.glsl"

void main() {
    vsOut.TexCoords = inPosition;

    gl_Position = (bindlessBuffersCamera[globalData.cameraBufferIndex].camera.projection * mat4(mat3(bindlessBuffersCamera[globalData.cameraBufferIndex].camera.view)) * vec4(inPosition, 1.0)).xyww;
}
