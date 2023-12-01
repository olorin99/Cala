
layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} fsIn;

layout (location = 0) out vec4 FragColour;

#include "shaderBridge.h"
#include "bindings.glsl"

layout (push_constant) uniform IBLData {
    LightGridBuffer lightGridBuffer;
    LightIndicesBuffer lightIndicesBuffer;
};

#include "util.glsl"

MATERIAL_DATA;

MATERIAL_DEFINITION;

MATERIAL_LOAD;

MATERIAL_EVAL;

INCLUDES_GO_HERE;

layout (set = CALA_MATERIAL_SET, binding = CALA_MATERIAL_BINDING) readonly buffer MatData {
    MaterialData materials[];
};

void main() {
    GPUMesh mesh = globalData.meshBuffer.meshData[fsIn.drawID];
    MaterialData materialData = materials[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    FragColour = evalMaterial(material);
}