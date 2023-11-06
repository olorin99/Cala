#ifndef LIGHT_DATA_GLSL
#define LIGHT_DATA_GLSL

struct LightGrid {
    uint offset;
    uint count;
};

//layout (set = 0, binding = 1) buffer LightGridSSBO {
//    LightGrid lightGrid[];
//} bindlessBuffersLightGrid[];

layout (std430, buffer_reference, buffer_reference_align = 8) readonly buffer LightGridBuffer {
    LightGrid lightGrid[];
};

//layout (set = 0, binding = 1) buffer BindlessLightIndices {
//    uint lightIndices[];
//} bindlessBuffersLightIndices[];

layout (std430, buffer_reference, buffer_reference_align = 8) readonly buffer LightIndicesBuffer {
    uint lightIndices[];
};

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

//layout (set = 0, binding = 1) readonly buffer LightData {
//    Light lights[];
//} bindlessBuffersLights[];

layout (std430, buffer_reference, buffer_reference_align = 8) readonly buffer LightBuffer {
    Light lights[];
};

//layout (set = 0, binding = 1) readonly buffer LightCount {
//    uint directLightCount;
//    uint pointLightCount;
//} bindlessBuffersLightCount[];

layout (std430, buffer_reference, buffer_reference_align = 8) readonly buffer LightCountBuffer {
    uint directLightCount;
    uint pointLightCount;
};

//uint getTotalLightCount() {
//    return bindlessBuffersLightCount[globalData.lightCountBufferIndex].directLightCount + bindlessBuffersLightCount[globalData.lightCountBufferIndex].pointLightCount;
//}

#endif