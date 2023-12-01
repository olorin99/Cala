#ifndef SHADER_UTIL_GLSL
#define SHADER_UTIL_GLSL

float linearDepth(float depth, float near, float far) {
    float depthRange = depth;
    return 2.0 * near * far / (far + near - depthRange * (far - near));
}

uint getTileIndex(vec3 position, uvec4 tileSizes, uvec2 screenSize, float near, float far) {
    uvec2 tileSize = screenSize / tileSizes.xy;
    float scale = 24.0 / log2(far / near);
    float bias = -(24.0 * log2(near) / log2(far / near));
    uint zTile = uint(max(log2(linearDepth(position.z, near, far)) * scale + bias, 0.0));
    uvec3 tiles = uvec3(uvec2(position.xy / tileSize), zTile);
    uint tileIndex = tiles.x + tileSizes.x * tiles.y + (tileSizes.x * tileSizes.y) * tiles.z;
    return tileIndex;
}

#endif