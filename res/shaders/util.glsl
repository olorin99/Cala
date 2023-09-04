float linearDepth(float depth) {
    float depthRange = depth;
    return 2.0 * globalBuffersCamera[globalData.cameraBufferIndex].camera.near * globalBuffersCamera[globalData.cameraBufferIndex].camera.far / (globalBuffersCamera[globalData.cameraBufferIndex].camera.far + globalBuffersCamera[globalData.cameraBufferIndex].camera.near - depthRange * (globalBuffersCamera[globalData.cameraBufferIndex].camera.far - globalBuffersCamera[globalData.cameraBufferIndex].camera.near));
}

uint getTileIndex() {
    uvec2 tileSize = screenSize / tileSizes.xy;
    float scale = 24.0 / log2(globalBuffersCamera[globalData.cameraBufferIndex].camera.far / globalBuffersCamera[globalData.cameraBufferIndex].camera.near);
    float bias = -(24.0 * log2(globalBuffersCamera[globalData.cameraBufferIndex].camera.near) / log2(globalBuffersCamera[globalData.cameraBufferIndex].camera.far / globalBuffersCamera[globalData.cameraBufferIndex].camera.near));
    uint zTile = uint(max(log2(linearDepth(gl_FragCoord.z)) * scale + bias, 0.0));
    uvec3 tiles = uvec3(uvec2(gl_FragCoord.xy / tileSize), zTile);
    uint tileIndex = tiles.x + tileSizes.x * tiles.y + (tileSizes.x * tileSizes.y) * tiles.z;
    return tileIndex;
}