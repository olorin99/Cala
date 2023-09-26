
layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform samplerCube cubeMaps[];
layout (set = 0, binding = 0) uniform sampler2D textureMaps[];
layout (set = 0, binding = 0) uniform image3D voxelGrid[];

#include "camera.glsl"

#include "global_data.glsl"

layout (push_constant) uniform PushData {
    int voxelGridIndex;
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
    return 0.5f * p + vec3(0.5);
}

bool isInsideCube(vec3 p, float e) {
    return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e;
}

void main() {
    if (!isInsideCube(fsIn.FragPos, 0)) {
        return;
    }

    Mesh mesh = bindlessBufferMesh[globalData.meshBufferIndex].meshData[fsIn.drawID];
    MaterialData materialData = materials[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    vec4 colour = evalMaterial(material);

    vec3 voxel = scaleAndBias(fsIn.FragPos);
    ivec3 dim = imageSize(voxelGrid[voxelGridIndex]);
    imageStore(voxelGrid[voxelGridIndex], ivec3(dim * voxel), colour);
}