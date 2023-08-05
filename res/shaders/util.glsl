float linearDepth(float depth) {
    float depthRange = depth;
    return 2.0 * camera.near * camera.far / (camera.far + camera.near - depthRange * (camera.far - camera.near));
}

uint getTileIndex() {
    uvec2 tileSize = screenSize / tileSizes.xy;
    float scale = 24.0 / log2(camera.far / camera.near);
    float bias = -(24.0 * log2(camera.near) / log2(camera.far / camera.near));
    uint zTile = uint(max(log2(linearDepth(gl_FragCoord.z)) * scale + bias, 0.0));
    uvec3 tiles = uvec3(uvec2(gl_FragCoord.xy / tileSize), zTile);
    uint tileIndex = tiles.x + tileSizes.x * tiles.y + (tileSizes.x * tileSizes.y) * tiles.z;
    return tileIndex;
}