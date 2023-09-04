
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
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

layout (set = 4, binding = 0) readonly buffer ModelData {
    mat4 transforms[];
};

void main() {
    mat4 model = transforms[gl_BaseInstance];
    vec3 T = normalize(mat3(model) * inTangent.xyz) * inTangent.w;
    vec3 N = normalize(mat3(model) * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    CameraData camera = globalBuffersCamera[globalData.cameraBufferIndex].camera;

    vsOut.FragPos = (model * vec4(inPosition, 1.0)).xyz;
    vsOut.TexCoords = inTexCoords;
    vsOut.TBN = mat3(T, B, N);
    vsOut.ViewPos = camera.position;
    vsOut.drawID = gl_BaseInstance;

    gl_Position = camera.projection * camera.view * model * vec4(inPosition, 1.0);
}