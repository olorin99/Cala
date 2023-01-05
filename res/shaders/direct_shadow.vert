#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec3 inBitangent;

layout (location = 0) out VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec4 WorldPosLightSpace;
} vsOut;

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
};

layout (set = 0, binding = 0) uniform FrameData {
    CameraData camera;
};

layout (set = 0, binding = 1) uniform LightData {
    CameraData lightCam;
};

layout (set = 1, binding = 0) uniform ModelData {
    mat4 model;
};

const mat4 bias = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0 );

void main() {
    vec3 T = normalize(vec3(model * vec4(inTangent, 0.0)));
    vec3 N = normalize(vec3(model * vec4(inNormal, 0.0)));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    vsOut.FragPos = (model * vec4(inPosition, 1.0)).xyz;
    vsOut.TexCoords = inTexCoords;
    vsOut.TBN = mat3(T, B, N);
    vsOut.ViewPos = camera.position;
    vsOut.WorldPosLightSpace = bias * lightCam.projection * lightCam.view * model * vec4(inPosition, 1.0);

    gl_Position = camera.projection * camera.view * model * vec4(inPosition, 1.0);
}