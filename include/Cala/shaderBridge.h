#ifndef SHADER_INCLUDE_GLSL
#define SHADER_INCLUDE_GLSL

#ifdef __cplusplus
#include "Ende/include/Ende/platform.h"
#include "Ende/include/Ende/math/Vec.h"
#include "Ende/include/Ende/math/Mat.h"

using uint = u32;
using uvec2 = ende::math::Vec<2, u32>;
using uvec3 = ende::math::Vec<3, u32>;
using uvec4 = ende::math::Vec<4, u32>;

using ivec2 = ende::math::Vec<2, i32>;
using ivec3 = ende::math::Vec<3, i32>;
using ivec4 = ende::math::Vec<4, i32>;

using vec2 = ende::math::Vec<2, f32>;
using vec3 = ende::math::Vec<3, f32>;
using vec4 = ende::math::Vec<4, f32>;

using mat2 = ende::math::Mat<2, f32>;
using mat3 = ende::math::Mat<3, f32>;
using mat4 = ende::math::Mat<4, f32>;
#endif

struct DispatchCommand {
    uint x;
    uint y;
    uint z;
};

struct MeshTaskCommand {
    uint x;
    uint y;
    uint z;
    uint meshID;
    uint meshLOD;
};

struct Meshlet {
    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint primitiveOffset;
    uint primitiveCount;
    vec3 center;
    float radius;
    vec3 coneApex;
    vec3 coneAxis;
    float coneCutoff;
};

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer MeshletBuffer {
    Meshlet meshlets[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer PrimitiveBuffer {
    uint8_t primitives[];
};
#else
#define MeshletBuffer u64
#define PrimitiveBuffer u64
#endif

#define MAX_LODS 10

struct LOD {
    uint meshletOffset;
    uint meshletCount;
};

struct GPUMesh {
    uint materialID;
    uint materialIndex;
    vec4 min;
    vec4 max;
    uint enabled;
    uint castShadows;
    uint lodCount;
    LOD lods[MAX_LODS];
};

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer MeshBuffer {
    GPUMesh meshData[];
};
#else
#define MeshBuffer u64
#endif

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer TransformsBuffer {
    mat4 transforms[];
};
#else
#define TransformsBuffer u64
#endif

struct Frustum {
    vec4 planes[6];
    vec4 corners[8];
};

struct GPUCamera {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    float exposure;
    Frustum frustum;
};

//layout (set = 0, binding = 1) buffer BindlessCameraBuffer { CameraData camera; } bindlessBuffersCamera[];

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer CameraBuffer {
    GPUCamera camera;
};
#else
#define CameraBuffer u64
#endif

struct LightGrid {
    uint offset;
    uint count;
};

#define MAX_CASCADES 7

struct Cascade {
    float distance;
    int shadowMapIndex;
};

struct GPULight {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float size;
    float shadowRange;
    float shadowBias;
    int shadowIndex;
    int cameraIndex;
    uint cascadeCount;
    Cascade cascades[MAX_CASCADES];
};

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) buffer LightGridBuffer {
    LightGrid lightGrid[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) buffer LightIndicesBuffer {
    uint lightIndices[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer LightBuffer {
    uint lightCount;
    GPULight lights[];
};
#else
#define LightGridBuffer u64
#define LightIndicesBuffer u64
#define LightBuffer u64
#endif

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 texCoords;
};

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) buffer VertexBuffer {
    Vertex vertices[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) buffer IndexBuffer {
    uint indices[];
};
#else
#define VertexBuffer u64
#define IndexBuffer u64
#endif



struct FeedbackInfo {
    uint drawnMeshes;
    uint totalMeshes;
    uint drawnMeshlets;
    uint totalMeshlets;
    uint drawnTriangles;
    uint meshletID;
    uint meshID;
};

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) buffer FeedbackBuffer {
    FeedbackInfo feedback;
};
#else
#define FeedbackBuffer u64
#endif

struct GlobalData {
    float gamma;
    uint time;
    uint gpuCulling;
    uint maxDrawCount;
    uvec4 tileSizes;
    uvec2 swapchainSize;
    uvec2 cursorPos;
    vec2 randomOffset;
    int poissonIndex;
    float bloomStrength;
    int shadowMode;
    int pcfSamples;
    int blockerSamples;
    float lodTransitionBase;
    float lodTransitionStep;
    uint lodBias;
    uint shadowLodBias;
    int irradianceIndex;
    int prefilterIndex;
    int brdfIndex;
    int nearestRepeatSampler;
    int linearRepeatSampler;
    int lodSampler;
    int shadowSampler;
    int primaryCameraIndex;
    int cullingCameraIndex;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    MeshletBuffer meshletBuffer;
    PrimitiveBuffer primitiveBuffer;
    MeshBuffer meshBuffer;
    TransformsBuffer transformsBuffer;
    CameraBuffer cameraBuffer;
    LightBuffer lightBuffer;
    LightGridBuffer lightGridBuffer;
    LightIndicesBuffer lightIndicesBuffer;
    FeedbackBuffer feedbackBuffer;
};

#define CALA_GLOBAL_DATA_SET 1
#define CALA_GLOBAL_DATA_BINDING 0

#ifndef __cplusplus
layout (set = CALA_GLOBAL_DATA_SET, binding = CALA_GLOBAL_DATA_BINDING) uniform Global {
    GlobalData globalData;
};
#endif

#define CALA_MATERIAL_SET 2
#define CALA_MATERIAL_BINDING 0

#define CALA_BINDLESS_SAMPLED_IMAGE 0
#define CALA_BINDLESS_STORAGE_IMAGE 1
#define CALA_BINDLESS_SAMPLERS 2
#define CALA_BINDLESS_BUFFERS 3

#endif