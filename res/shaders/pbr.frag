#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
} fsIn;

layout (location = 0) out vec4 FragColour;

//layout (set = 2, binding = 0) uniform sampler2D albedoMap;
//layout (set = 2, binding = 1) uniform sampler2D normalMap;
//layout (set = 2, binding = 2) uniform sampler2D metallicMap;
//layout (set = 2, binding = 3) uniform sampler2D roughnessMap;
//layout (set = 2, binding = 4) uniform sampler2D aoMap;

layout (set = 0, binding = 0) uniform samplerCube pointShadows[];
layout (set = 0, binding = 0) uniform sampler2D directShadows[];

struct MaterialData {
    uint albedoIndex;
    uint normalIndex;
    uint metallicIndex;
    uint roughnessIndex;
    uint aoIndex;
};

layout (set = 2, binding = 0) uniform MatData {
    MaterialData material;
};

//layout (set = 2, binding = 5) uniform samplerCube irradianceMap;
//layout (set = 2, binding = 6) uniform samplerCube prefilterMap;
//layout (set = 2, binding = 7) uniform sampler2D brdfLUT;

struct Light {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float range;
    float linear;
    float quadratic;
//    float radius;
    uint shadowIndex;
};

layout (set = 3, binding = 0) readonly buffer LightData {
    Light lights[];
};

layout (set = 3, binding = 1) readonly buffer LightCount {
    uint directLightCount;
    uint pointLightCount;
    float shadowBias;
};

const float PI = 3.1415926559;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float calcShadows(uint index, vec3 viewDir, vec3 offset, float bias, float range) {
    float shadow = 1.0;
    if(texture(pointShadows[index], viewDir + offset).r < length(viewDir) / range - bias) {
        shadow = 0.0;
    }
    return shadow;
}

vec3 sampleOffsetDirections[20] = vec3[](
vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

float filterPCF(uint index, vec3 lightPos, float bias, float range) {
    float shadow = 0;
    int samples = 20;
    vec3 viewDir = fsIn.FragPos - lightPos;
    float viewDistance = length(viewDir);
    float diskRadius = (1.0 + (viewDistance / 100.f)) / 25.0;

    for (int i = 0; i < samples; i++) {
        shadow += calcShadows(index, viewDir, sampleOffsetDirections[i] * diskRadius, bias, range);
    }
    return shadow / samples;
}

void main() {

//    vec3 albedo = texture(albedoMap, fsIn.TexCoords).rgb;
//    vec3 normal = texture(normalMap, fsIn.TexCoords).rgb;
//    float metallic = texture(metallicMap, fsIn.TexCoords).r;
//    float roughness = texture(roughnessMap, fsIn.TexCoords).r;
//    float ao = texture(aoMap, fsIn.TexCoords).r;

    vec3 albedo = texture(directShadows[material.albedoIndex], fsIn.TexCoords).rgb;
    vec3 normal = texture(directShadows[material.normalIndex], fsIn.TexCoords).rgb;
    float metallic = texture(directShadows[material.metallicIndex], fsIn.TexCoords).r;
    float roughness = texture(directShadows[material.roughnessIndex], fsIn.TexCoords).r;
    float ao = texture(directShadows[material.aoIndex], fsIn.TexCoords).r;

    normal = normalize(normal * 2.0 - 1.0);
    normal = normalize(fsIn.TBN * normal);

    vec3 faceNormal = fsIn.TBN[2];

    vec3 viewPos = fsIn.ViewPos * vec3(-1, 1, -1);

    vec3 V = normalize(viewPos - fsIn.FragPos);
    vec3 R = reflect(-V, normal);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    uint lightNum = min(directLightCount + pointLightCount, 1000);
    for (uint i = 0; i < lightNum; i++) {
        Light light = lights[i];
        float shadow = 0.0;
        vec3 L = vec3(0.0);
        if (light.type > 0) {
            L = normalize(light.position - fsIn.FragPos);
            float bias = max(shadowBias * (1.0 - dot(normal, L)), 0.0001);
            shadow = filterPCF(light.shadowIndex, light.position, bias, light.range);
        } else {
            L = normalize(-light.position);
            shadow = 1;
        }
        vec3 H = normalize(V + L);
        float distance = length(light.position.xyz - fsIn.FragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = light.colour * light.intensity * attenuation;

        float NDF = distributionGGX(normal, H, roughness);
        float G = geometrySmith(normal, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 nominator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
        vec3 specular = nominator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        float NdotL = max(dot(normal, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;

    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 colour = (ambient + Lo);

    colour = colour / (colour + vec3(1.0));
    colour = pow(colour, vec3(1.0 / 2.2));

    FragColour = vec4(colour, 1.0);
}