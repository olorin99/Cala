
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec3 inTangent;

struct CameraData {
    mat4 viewProjection;
    vec3 position;
    float near;
    float far;
};

layout (push_constant) uniform PushConstants {
    CameraData camera;
};


#include "global_data.glsl"

layout (set = 0, binding = 1) readonly buffer TransformsBuffer {
    mat4 transforms[];
} globalBuffersTransforms[];

void main() {
    mat4 model = globalBuffersTransforms[globalData.transformsBufferIndex].transforms[gl_BaseInstance];
    gl_Position = camera.viewProjection * model * vec4(inPosition, 1.0);
}