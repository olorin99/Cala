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

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    float exposure;
};

layout (set = 1, binding = 0) uniform FrameData {
    CameraData camera;
};

layout (push_constant) uniform IBLData {
    uvec4 tileSizes;
    uvec2 screenSize;
    int irradianceIndex;
    int prefilteredIndex;
    int brdfIndex;
};

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
    uint _pad;
    vec4 min;
    vec4 max;
};

layout (set = 2, binding = 1) readonly buffer MeshData {
    Mesh meshData[];
};

struct LightGrid {
    uint offset;
    uint count;
};

layout (set = 2, binding = 2) buffer LightGridSSBO {
    LightGrid lightGrid[];
};

layout (set = 2, binding = 3) buffer LightIndices {
    uint globalLightIndices[];
};

struct Light {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float shadowRange;
    float radius;
    float quadratic;
    int shadowIndex;
};

layout (set = 3, binding = 0) readonly buffer LightData {
    Light lights[];
};

layout (set = 3, binding = 1) readonly buffer LightCount {
    uint directLightCount;
    uint pointLightCount;
    float shadowBias;
};

#include "pbr.glsl"
#include "shadow.glsl"

float linearDepth(float depth) {
    float depthRange = 2.0 * depth - 1.0;
    return 2.0 * camera.near * camera.far / (camera.far + camera.near - depthRange * (camera.far - camera.near));
}

vec3 pointLight(Light light, vec3 normal, vec3 viewPos, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 lightVec = light.position - fsIn.FragPos;
    vec3 L = normalize(lightVec);
    float NdotL = max(dot(normal, L), 0.0);

    float shadow = 1.0;

    if (light.shadowIndex >= 0) {
        float bias = max(shadowBias * (1.0 - dot(normal, L)), 0.0001);
        shadow = filterPCF(light.shadowIndex, light.position, bias, light.shadowRange);
    }

    if (shadow == 0)
        return vec3(0.0);

    float distanceSqared = dot(lightVec, lightVec);
    float rangeSquared = light.shadowRange * light.shadowRange;
    float dpr = distanceSqared / max(0.0001, rangeSquared);
    dpr *= dpr;
    float attenuation = clamp(1 - dpr, 0, 1.0) / max(0.0001, distanceSqared);

    vec3 H = normalize(V + L);
    vec3 radiance = light.colour * light.intensity * attenuation;

    float NDF = distributionGGX(normal, H, roughness);
    float G = geometrySmith(normal, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 nominator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, V), 0.0) * NdotL + 0.0001;
    vec3 specular = nominator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    return (kD * albedo / PI + specular) * radiance * NdotL * shadow;
}

void main() {
    Mesh mesh = meshData[fsIn.drawID];
    MaterialData materialData = material[mesh.materialIndex];

    vec4 albedaRGBA = texture(textureMaps[materialData.albedoIndex], fsIn.TexCoords);

    if (albedaRGBA.a < 0.5)
        discard;

    vec3 albedo = albedaRGBA.rgb;
    vec3 normal = texture(textureMaps[materialData.normalIndex], fsIn.TexCoords).rgb;
    float metallic = texture(textureMaps[materialData.metallicRoughnessIndex], fsIn.TexCoords).b;
    float roughness = texture(textureMaps[materialData.metallicRoughnessIndex], fsIn.TexCoords).g;

    normal = normalize(normal * 2.0 - 1.0);
    normal = normalize(fsIn.TBN * normal);

    vec3 viewPos = fsIn.ViewPos;
    vec3 V = normalize(viewPos - fsIn.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 Lo = vec3(0.0);

    uvec2 tileSize = screenSize / tileSizes.xy;
    float scale = 24.0 / log2(camera.far / camera.near);
    float bias = -(24.0 * log2(camera.near) / log2(camera.far / camera.near));
    uint zTile = uint(max(log2(linearDepth(gl_FragCoord.z)) * scale + bias, 0.0));
    uvec3 tiles = uvec3(uvec2(gl_FragCoord.xy / tileSize), zTile);
    uint tileIndex = tiles.x + tileSizes.x * tiles.y + (tileSizes.x * tileSizes.y) * tiles.z;

    uint lightCount = lightGrid[tileIndex].count;
    uint lightOffset = lightGrid[tileIndex].offset;

    for (uint i = 0; i < lightCount; i++) {
        Light light = lights[globalLightIndices[lightOffset + i]];
        Lo += pointLight(light, normal, viewPos, V, F0, albedo, roughness, metallic);
    }

    vec3 ambient = getAmbient(irradianceIndex, prefilteredIndex, brdfIndex, normal, V, F0, albedo, roughness, metallic);
    vec3 colour = (ambient + Lo);

    FragColour = vec4(colour, 1.0);
//    FragColour = vec4(tiles, 1.0);
}