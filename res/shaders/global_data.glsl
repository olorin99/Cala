#ifndef GLOBAL_DATA_GLSL
#define GLOBAL_DATA_GLSL

#include "camera.glsl"
#include "mesh_data.glsl"
#include "transforms_data.glsl"
#include "light_data.glsl"

struct GlobalData {
    float gamma;
    uint time;
    uint gpuCulling;
    uint maxDrawCount;
    uvec2 swapchainSize;
    int irradianceIndex;
    int prefilterIndex;
    int brdfIndex;
    int nearestRepeatSampler;
    int linearRepeatSampler;
    int lodSampler;
    int shadowSampler;
    CameraBuffer cameraBuffer;
    MeshBuffer meshBuffer;
    TransformsBuffer transformsBuffer;
    LightBuffer lightBuffer;
    LightCountBuffer lightCountBuffer;
};

layout (set = 1, binding = 0) uniform Global {
    GlobalData globalData;
};

#endif