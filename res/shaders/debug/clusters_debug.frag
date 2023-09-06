
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

#include "camera.glsl"

#include "global_data.glsl"

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

    CameraData camera = globalBuffersCamera[globalData.cameraBufferIndex].camera;

    const float maxLightCount = 250;

    float depth = texture(depthMap, fsIn.TexCoords).r;
    uint tileIndex = getTileIndex(gl_FragCoord.xy, depth, camera.near, camera.far);

    uint lightCount = 0;

    lightCount += lightGrid[tileIndex].count;

    float fullness = lightCount / maxLightCount;

    FragColour = vec4(fullness, 1.0 - fullness, 0.0, 1.0);
}