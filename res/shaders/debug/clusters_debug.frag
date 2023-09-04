
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

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

layout (push_constant) uniform TileData {
    uvec4 tileSizes;
    uvec2 screenSize;
};

struct LightGrid {
    uint offset;
    uint count;
};

layout (set = 1, binding = 1) buffer LightGridSSBO {
    LightGrid lightGrid[];
};

layout (set = 1, binding = 2) uniform sampler2D depthMap;

#include "util.glsl"

void main() {

    const float maxLightCount = 250;

    float depth = texture(depthMap, fsIn.TexCoords).r;
    uint tileIndex = getTileIndex(gl_FragCoord.xy, depth);

    uint lightCount = 0;

    lightCount += lightGrid[tileIndex].count;

    float fullness = lightCount / maxLightCount;

    FragColour = vec4(fullness, 1.0 - fullness, 0.0, 1.0);
}