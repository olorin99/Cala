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

layout (push_constant) uniform IBLData {
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

struct Light {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float range;
    float linear;
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

vec3 pointLight(Light light, vec3 normal, vec3 viewPos, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 lightVec = light.position - fsIn.FragPos;
    vec3 L = normalize(lightVec);
    float NdotL = max(dot(normal, L), 0.0);

    float shadow = 1.0;

    if (light.shadowIndex >= 0) {
        float bias = max(shadowBias * (1.0 - dot(normal, L)), 0.0001);
        shadow = filterPCF(light.shadowIndex, light.position, bias, light.range);
    }

    if (shadow == 0)
        return vec3(0.0);

    float distanceSqared = dot(lightVec, lightVec);
    float attenuation = 1.0 / max(distanceSqared, 0.1);
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

    uint lightNum = min(directLightCount + pointLightCount, 1000);
    for (uint i = 0; i < lightNum; i++) {
        Light light = lights[i];
        Lo += pointLight(light, normal, viewPos, V, F0, albedo, roughness, metallic);
    }

    vec3 ambient = getAmbient(irradianceIndex, prefilteredIndex, brdfIndex, normal, V, F0, albedo, roughness, metallic);
    vec3 colour = (ambient + Lo);

    FragColour = vec4(colour, 1.0);
}