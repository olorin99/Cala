#ifndef GLOBAL_DATA_GLSL
#define GLOBAL_DATA_GLSL

struct VXGIData {
    mat4 projection;
    uvec3 gridDimensions;
    vec3 voxelExtent;
    int gridIndex;
};

struct GlobalData {
    float gamma;
    uint time;
    uint gpuCulling;
    uint maxDrawCount;
    uvec2 swapchainSize;
    int transformsBufferIndex;
    int meshBufferIndex;
    int lightBufferIndex;
    int lightCountBufferIndex;
    int cameraBufferIndex;
    int irradianceIndex;
    int prefilterIndex;
    int brdfIndex;
    int nearestRepeatSampler;
    int linearRepeatSampler;
    int lodSampler;
    int shadowSampler;
    VXGIData vxgi;
};

layout (std430, set = 1, binding = 0) readonly buffer Global {
    GlobalData globalData;
};

#endif