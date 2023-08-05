#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

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

MATERIAL_DATA;

MATERIAL_DEFINITION;

MATERIAL_LOAD;

layout (set = 2, binding = 0) readonly buffer MatData {
    MaterialData material[];
};

struct Mesh {
    uint firstIndex;
    uint indexCount;
    uint materialIndex;
    uint materialInstanceIndex;
    vec4 min;
    vec4 max;
};

layout (set = 2, binding = 1) readonly buffer MeshData {
    Mesh meshData[];
};

void main() {
    Mesh mesh = meshData[fsIn.drawID];
    MaterialData materialData = material[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    FragColour = vec4(material.metallic, 0.0, 0.0, 1.0);
}