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

float random(vec2 uv) {
    vec2 noise = (fract(vec2(sin(dot(uv, vec2(12.9898,78.233)*2.0)) * 43758.5453)));
    return abs(noise.x + noise.y) * 0.5;
}

vec2 random2(vec2 uv) {
    float noiseX = (fract(sin(dot(uv, vec2(12.9898,78.233) * 2.0)) * 43758.5453));
    float noiseY = sqrt(1 - noiseX * noiseX);
    return vec2(noiseX, noiseY);
}

//float random(vec2 uv) {
//    vec2 K1 = vec2(
//    23.14069263277926, // e^pi (Gelfond's constant)
//    2.665144142690225 // 2^sqrt(2) (Gelfondâ€“Schneider constant)
//    );
//    return fract(cos(dot(uv,K1)) * 12345.6789);
//}

//vec2 random2(vec2 uv) {
//    return vec2(random(uv), random(uv.yx));
//}

//vec2 random(vec2 uv) {
//    float noiseX = (frac(sin(dot(uv, vec2(12.9898,78.233)      )) * 43758.5453));
//    float noiseY = (frac(sin(dot(uv, vec2(12.9898,78.233) * 2.0)) * 43758.5453));
//    return vec2(noiseX, noiseY) * 0.004;
//}

#endif