
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

#include "camera.glsl"

#include "global_data.glsl"

layout (push_constant) uniform IBLData {
    uvec4 tileSizes;
    uvec2 screenSize;
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

void main() {
    Mesh mesh = bindlessBufferMesh[globalData.meshBufferIndex].meshData[fsIn.drawID];
    MaterialData materialData = materials[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    FragColour = evalMaterial(material);
}