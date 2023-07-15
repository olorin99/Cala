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

struct MaterialData {
    int albedoIndex;
    int normalIndex;
    int metallicRoughnessIndex;
};

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

struct Material {
    vec3 albedo;
    vec3 normal;
    float metallic;
    float roughness;
};

Material loadMaterial(MaterialData data) {
    Material material;
    if (data.albedoIndex < 0) {
        material.albedo = vec3(1.0);
    } else {
        vec4 albedaRGBA = texture(textureMaps[data.albedoIndex], fsIn.TexCoords);
        if (albedaRGBA.a < 0.5)
        discard;
        material.albedo = albedaRGBA.rgb;
    }

    if (data.normalIndex < 0) {
        material.normal = vec3(0.52, 0.52, 1);
    } else {
        material.normal = texture(textureMaps[data.normalIndex], fsIn.TexCoords).rgb;
    }
    material.normal = normalize(material.normal * 2.0 - 1.0);
    material.normal = normalize(fsIn.TBN * material.normal);

    if (data.metallicRoughnessIndex < 0) {
        material.roughness = 1.0;
        material.metallic = 0.0;
    } else {
        material.metallic = texture(textureMaps[data.metallicRoughnessIndex], fsIn.TexCoords).b;
        material.roughness = texture(textureMaps[data.metallicRoughnessIndex], fsIn.TexCoords).g;
    }
    return material;
}

void main() {
    Mesh mesh = meshData[fsIn.drawID];
    MaterialData materialData = material[mesh.materialInstanceIndex];

    Material material = loadMaterial(materialData);

    FragColour = vec4(material.roughness, 0.0, 0.0, 1.0);
//    FragColour = vec4(fsIn.TBN[2], 1.0);
}