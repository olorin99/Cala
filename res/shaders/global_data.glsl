#ifndef GLOBAL_DATA_GLSL
#define GLOBAL_DATA_GLSL

struct GlobalData {
    float gamma;
    uint time;
    uint maxDrawCount;
    int transformsBufferIndex;
    int meshBufferIndex;
    int materialBufferIndex;
    int lightBufferIndex;
    int cameraBufferIndex;
    int irradianceIndex;
    int prefilterIndex;
    int brdfIndex;
    int nearestRepeatSampler;
    int linearRepeatSampler;
    int lodSampler;
    int shadowSampler;
};

layout (set = 1, binding = 0) uniform Global {
    GlobalData globalData;
};

#endif