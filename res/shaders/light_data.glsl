#ifndef LIGHT_DATA_GLSL
#define LIGHT_DATA_GLSL

struct LightGrid {
    uint offset;
    uint count;
};

layout (scalar, buffer_reference, buffer_reference_align = 8) buffer LightGridBuffer {
    LightGrid lightGrid[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) buffer LightIndicesBuffer {
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

layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer LightBuffer {
    Light lights[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer LightCountBuffer {
    uint directLightCount;
    uint pointLightCount;
};

#endif