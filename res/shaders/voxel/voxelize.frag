
layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
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
    mat4 orthographic;
    uvec4 tileSizes;
    uvec2 screenSize;
    int lightGridIndex;
    int lightIndicesIndex;
    int voxelGridIndex;
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
    return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e;
}

void main() {
//    if (!isInsideCube(fsIn.FragPos, 0)) {
//        return;
//    }

    Mesh mesh = bindlessBufferMesh[globalData.meshBufferIndex].meshData[fsIn.drawID];
    MaterialData materialData = materials[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    vec4 colour = evalMaterial(material);

    vec3 voxelPos = scaleAndBias(fsIn.FragPos);
    ivec3 dim = imageSize(voxelGrid[voxelGridIndex]);
    ivec3 voxelCoords = ivec3(dim * voxelPos);
    if (voxelCoords.x > dim.x || voxelCoords.y > dim.y || voxelCoords.x < 0 || voxelCoords.y < 0)
        return;
    voxelCoords = min(voxelCoords, dim);
    imageStore(voxelGrid[voxelGridIndex], voxelCoords, colour);
//    FragColour = ivec4(dim * voxel, 1.0);
//    FragColour = colour;
//    FragColour = vec4(fsIn.FragPos, 1.0);
    FragColour = vec4(voxelPos, 1.0);
//    FragColour = vec4(gl_FragCoord.xyz, 1.0);
//    FragColour = vec4(gl_FragCoord.xy, gl_FragCoord.z * dim.z, 1.0);
//    FragColour = vec4(dim, 1.0);
//    FragColour = vec4(dim * voxel, 1.0);
}