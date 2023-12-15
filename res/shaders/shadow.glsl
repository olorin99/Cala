#ifndef SHADOW_GLSL
#define SHADOW_GLSL

CALA_USE_SAMPLED_IMAGE(Cube)
CALA_USE_SAMPLED_IMAGE(2D)

float calcShadows(uint index, vec3 viewDir, vec3 offset, float bias, float range) {
    float shadow = 1.0;
    if(texture(CALA_COMBINED_SAMPLERCUBE(index, globalData.shadowSampler), viewDir + offset).r < length(viewDir) / range - bias) {
        shadow = 0.0;
    }
    return shadow;
}

float sampleShadow(int index, vec3 shadowCoords, vec2 offset, float bias) {
    float closestDepth = texture(CALA_COMBINED_SAMPLER2D(index, globalData.shadowSampler), shadowCoords.xy + offset).r;
    if (closestDepth < shadowCoords.z - bias)
        return 0.0;
    return 1.0;
}

vec3 sampleOffsetDirections[20] = vec3[](
vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

float filterPCF(uint index, vec3 lightPos, float bias, float range) {
    float shadow = 0;
    int samples = 20;
    vec3 viewDir = fsIn.FragPos - lightPos;
    float viewDistance = length(viewDir);
    float diskRadius = (1.0 + (viewDistance / 100.f)) / 25.0;

    for (int i = 0; i < samples; i++) {
        shadow += calcShadows(index, viewDir, sampleOffsetDirections[i] * diskRadius, bias, range);
    }
    return shadow / samples;
}

float filterPCF2D(int index, vec3 lightPos, vec3 shadowCoords, float bias) {
//    if (lightPos.z > 1.0)
//        return 0.0;
    float shadow = 0;
    int samples = 20;
    vec2 texelSize = 1.0 / textureSize(CALA_COMBINED_SAMPLER2D(index, globalData.shadowSampler), 0);
    float diskRadius = (1.0 + (length(shadowCoords) / 100.f)) / 25.0;

    for (int i = 0; i < samples; i++) {
        shadow += sampleShadow(index, shadowCoords, sampleOffsetDirections[i].xy * texelSize, bias);
    }
//    for(int x = -1; x <= 1; ++x)
//    {
//        for(int y = -1; y <= 1; ++y)
//        {
//            shadow += sampleShadow(index, shadowCoords, vec2(x, y) * texelSize, bias);
//        }
//    }
//    shadow /= 9.0;
    return shadow / samples;
}

#endif