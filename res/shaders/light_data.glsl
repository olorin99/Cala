#ifndef LIGHT_DATA_GLSL
#define LIGHT_DATA_GLSL

struct LightGrid {
    uint offset;
    uint count;
};

layout (set = 0, binding = 1) buffer LightGridSSBO {
    LightGrid lightGrid[];
} bindlessBuffersLightGrid[];

layout (set = 0, binding = 1) buffer LightIndices {
    uint lightIndices[];
} bindlessBuffersLightIndices[];

struct Light {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float shadowRange;
    float radius;
    float shadowBias;
    int shadowIndex;
};

layout (set = 0, binding = 1) readonly buffer LightData {
    Light lights[];
} bindlessBuffersLights[];

layout (set = 0, binding = 1) readonly buffer LightCount {
    uint directLightCount;
    uint pointLightCount;
} bindlessBuffersLightCount[];

uint getTotalLightCount() {
    return bindlessBuffersLightCount[globalData.lightCountBufferIndex].directLightCount + bindlessBuffersLightCount[globalData.lightCountBufferIndex].pointLightCount;
}

#endif