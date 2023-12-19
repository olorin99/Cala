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

float filterPCF2D(int index, vec3 shadowCoords, float radius, float bias) {
    float shadow = 0;
    int samples = globalData.pcfSamples;
    vec2 texelSize = 1.0 / textureSize(CALA_COMBINED_SAMPLER2D(index, globalData.shadowSampler), 0);

    for (int i = 0; i < samples; i++) {
//        vec2 randomDirection = random2(globalData.randomOffset * gl_FragCoord.xy * i) * 2 - 1;
        vec2 randomDirection = poisson(i / float(samples));
        shadow += sampleShadow(index, shadowCoords, randomDirection * texelSize * radius, bias);
//        shadow += sampleShadow(index, shadowCoords, sampleOffsetDirections[i].xy * texelSize * radius, bias);
    }
    return shadow / samples;
}

float blockerDistance(int shadowMapIndex, vec3 shadowCoords, float lightSize) {
    int blockers = 0;
    float avgDistance = 0;
    float searchWidth = lightSize * (shadowCoords.z - 0.1) / globalData.cameraBuffer[globalData.primaryCameraIndex].camera.position.z;
    vec2 texelSize = 1.0 / textureSize(CALA_COMBINED_SAMPLER2D(shadowMapIndex, globalData.shadowSampler), 0);
    int samples = globalData.blockerSamples;

    for (int i = 0; i < samples; i++) {
//        vec2 randomDirection = random2(globalData.randomOffset * gl_FragCoord.xy * i) * 2 - 1;
        vec2 randomDirection = poisson(i / float(samples));
        float z = texture(CALA_COMBINED_SAMPLER2D(shadowMapIndex, globalData.shadowSampler), shadowCoords.xy + randomDirection * texelSize * searchWidth).r;
//        float z = texture(CALA_COMBINED_SAMPLER2D(shadowMapIndex, globalData.shadowSampler), shadowCoords.xy + sampleOffsetDirections[i].xy * texelSize * searchWidth).r;
        if (z < shadowCoords.z) {
            blockers++;
            avgDistance += z;
        }
    }
    if (blockers == 0)
        return -1;
    return avgDistance / blockers;
}

float pcss2D(int index, vec3 shadowCoords, float lightSize, float bias) {
    float blockerDist = blockerDistance(index, shadowCoords, lightSize);
    if (blockerDist < 0)
        return 1;
    float penumbraSize = lightSize * (shadowCoords.z - blockerDist) / blockerDist;
    return filterPCF2D(index, shadowCoords, penumbraSize, bias);
}

#endif