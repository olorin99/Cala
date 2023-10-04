
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec3 ClipPos;
    flat uint drawID;
} vsOut;

#include "camera.glsl"

#include "global_data.glsl"

#include "transforms_data.glsl"

layout (push_constant) uniform PushData {
    mat4 orthographic;
    uvec4 tileSizes;
    uvec2 screenSize;
    int lightGridIndex;
    int lightIndicesIndex;
    int voxelGridIndex;
};

void main() {
    mat4 model = bindlessBuffersTransforms[globalData.transformsBufferIndex].transforms[gl_BaseInstance];

    vec3 T = normalize(mat3(model) * inTangent.xyz) * inTangent.w;
    vec3 N = normalize(mat3(model) * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    CameraData camera = bindlessBuffersCamera[globalData.cameraBufferIndex].camera;

    vsOut.FragPos = (model * vec4(inPosition, 1.0)).xyz;
    vsOut.TexCoords = inTexCoords;
    vsOut.TBN = mat3(T, B, N);
    vsOut.ViewPos = camera.position;
    vsOut.ClipPos = (orthographic * model * vec4(inPosition, 1.0)).xyz;
    vsOut.drawID = gl_BaseInstance;

    gl_Position = orthographic * model * vec4(inPosition, 1.0);
}