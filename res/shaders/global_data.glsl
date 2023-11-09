#ifndef GLOBAL_DATA_GLSL
#define GLOBAL_DATA_GLSL

#include "mesh_data.glsl"
#include "transforms_data.glsl"
#include "camera.glsl"
#include "light_data.glsl"

struct GlobalData {
    float gamma;
    uint time;
    uint gpuCulling;
    uint maxDrawCount;
    uvec4 tileSizes;
    uvec2 swapchainSize;
    int irradianceIndex;
    int prefilterIndex;
    int brdfIndex;
    int nearestRepeatSampler;
    int linearRepeatSampler;
    int lodSampler;
    int shadowSampler;
    MeshBuffer meshBuffer;
    TransformsBuffer transformsBuffer;
    CameraBuffer cameraBuffer;
    LightBuffer lightBuffer;
    LightCountBuffer lightCountBuffer;
};

layout (set = 1, binding = 0) uniform Global {
    GlobalData globalData;
};

#endif