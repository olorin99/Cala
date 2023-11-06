
layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec3 ClipPos;
    flat uint drawID;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform textureCube cubeMaps[];
layout (set = 0, binding = 0) uniform texture2D textureMaps[];
layout (set = 0, binding = 2) uniform sampler samplers[];
layout (set = 0, binding = 3) uniform writeonly image3D voxelGrid[];

#include "camera.glsl"

#include "global_data.glsl"

layout (push_constant) uniform PushData {
    uvec4 voxelGridSize;
    uvec4 tileSizes;
    int lightGridIndex;
    int lightIndicesIndex;
};

#include "util.glsl"
#include "pbr.glsl"
#include "shadow.glsl"
#include "lighting.glsl"

MATERIAL_DATA;

MATERIAL_DEFINITION;

MATERIAL_LOAD;

MATERIAL_EVAL;

layout (set = 2, binding = 0) readonly buffer MatData {
    MaterialData materials[];
};

#include "mesh_data.glsl"

vec3 scaleAndBias(vec3 p) {
    return vec3(p.xy * 0.5 + 0.5, p.z);
}

bool isInsideCube(vec3 p, float e) {
    return p.x < 1 + e && p.y < 1 + e && p.z < 1 + e && p.x > 0 - e && p.y > 0 - e && p.z > 0 - e;
}

void main() {
    vec3 voxelPos = scaleAndBias(fsIn.ClipPos);
    if (!isInsideCube(voxelPos, 0))
        return;

    Mesh mesh = globalData.meshBuffer.meshData[fsIn.drawID];
    MaterialData materialData = materials[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    vec4 colour = evalMaterial(material);

    ivec3 dim = imageSize(voxelGrid[globalData.vxgi.gridIndex]);
    ivec3 voxelCoords = ivec3(dim * voxelPos);
    imageStore(voxelGrid[globalData.vxgi.gridIndex], voxelCoords, colour);
//    FragColour = vec4(voxelPos, 1.0);
    FragColour = vec4(voxelCoords, 1.0);
}