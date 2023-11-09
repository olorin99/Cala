
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

#include "camera.glsl"

#include "global_data.glsl"

layout (push_constant) uniform TileData {
    LightGridBuffer lightGridBuffer;
};

layout (set = 1, binding = 1) uniform sampler2D depthMap;

#include "util.glsl"

void main() {

    CameraData camera = globalData.cameraBuffer.camera;

    const float maxLightCount = 250;

    float depth = texture(depthMap, fsIn.TexCoords).r;
    uint tileIndex = getTileIndex(vec3(gl_FragCoord.xy, depth), globalData.tileSizes, globalData.swapchainSize, camera.near, camera.far);
    uint maxTileCount = globalData.tileSizes.x * globalData.tileSizes.y * globalData.tileSizes.z - 1;

    uint lightCount = lightGridBuffer.lightGrid[min(tileIndex, maxTileCount)].count;
    if (lightCount == 0)
        discard;

    float fullness = lightCount / maxLightCount;

    FragColour = vec4(fullness, 1.0 - fullness, 0.0, 1.0);
}