#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

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

layout (set = 1, binding = 0) uniform FrameData {
    CameraData camera;
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

float linearDepth(float depth) {
    float depthRange = 2.0 * depth - 1.0;
    return 2.0 * camera.near * camera.far / (camera.far + camera.near - depthRange * (camera.far - camera.near));
}

void main() {

    const float maxLightCount = 100;

    uvec2 tileSize = screenSize / tileSizes.xy;
    float scale = 24.0 / log2(camera.far / camera.near);
    float bias = -(24.0 * log2(camera.near) / log2(camera.far / camera.near));

    uint lightCount = 0;

    float depth = texture(depthMap, fsIn.TexCoords).r;

//    for (uint z = 0; z < 24; z++) {
        uint zTile = uint(max(log2(linearDepth(depth)) * scale + bias, 0.0));
        uvec3 tiles = uvec3(uvec2(gl_FragCoord.xy / tileSize), zTile);
        uint tileIndex = tiles.x + tileSizes.x * tiles.y + (tileSizes.x * tileSizes.y) * tiles.z;
        lightCount += lightGrid[tileIndex].count;
//    }

    float fullness = lightCount / maxLightCount;

//    FragColour = vec4(fullness, 1.0 - fullness, 0.0, 1.0);
//    FragColour = vec4(gl_FragCoord.xy, depth, 1.0);
    FragColour = vec4(tiles, tileIndex);
}