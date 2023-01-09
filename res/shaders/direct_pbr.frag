#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec4 WorldPosLightSpace;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D albedoMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D metallicMap;
layout (set = 2, binding = 3) uniform sampler2D roughnessMap;
layout (set = 2, binding = 4) uniform sampler2D aoMap;

layout (set = 2, binding = 5) uniform sampler2D shadowMap;

//layout (set = 2, binding = 5) uniform samplerCube irradianceMap;
//layout (set = 2, binding = 6) uniform samplerCube prefilterMap;
//layout (set = 2, binding = 7) uniform sampler2D brdfLUT;

struct DirectLight {
    vec3 direction;
    vec3 colour;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float radius;
};

layout (set = 3, binding = 0) readonly buffer LightData {
    DirectLight lights[];
};

layout (set = 3, binding = 1) readonly buffer LightCount {
    uint lightCount;
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

float calcShadows(vec3 fragPosLight, vec2 offset, float bias) {
    float shadow = 1.0;
    if(texture(shadowMap, fragPosLight.xy + offset).r < fragPosLight.z - bias) {
        shadow = 0.0;
    }
    if (fragPosLight.z > 1.0) {
        shadow = 1.0;
    }
    return shadow;
}

float filterPCF(vec3 fragPosLight, float bias) {
    ivec2 texDim = textureSize(shadowMap, 0);
    float scale = 1.5;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowFactor += calcShadows(fragPosLight, vec2(dx*x, dy*y), bias);
            count++;
        }

    }
    return shadowFactor / count;
}

void main() {

    vec3 albedo = texture(albedoMap, fsIn.TexCoords).rgb;
    vec3 normal = texture(normalMap, fsIn.TexCoords).rgb;
    float metallic = texture(metallicMap, fsIn.TexCoords).r;
    float roughness = texture(roughnessMap, fsIn.TexCoords).r;
    float ao = texture(aoMap, fsIn.TexCoords).r;

    normal = normalize(normal * 2.0 - 1.0);
    normal = normalize(fsIn.TBN * normal);

    vec3 viewPos = fsIn.ViewPos * vec3(-1, 1, -1);

    vec3 V = normalize(viewPos - fsIn.FragPos);
    vec3 R = reflect(-V, normal);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    float shadow = 1.0;

    uint lightNum = min(lightCount, 1000);
    for (uint i = 0; i < lightNum; i++) {
        DirectLight light = lights[i];
        vec3 L = normalize(-light.direction);
        vec3 H = normalize(V + L);
        vec3 radiance = light.colour * light.intensity;

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;

        //        float bias = max(0.01 * (1.0 - dot(normal, L)), 0.00001);
        //        shadow = filterPCF(fsIn.FragPos - light.position, bias);
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 colour = (ambient + Lo) * shadow;

    colour = colour / (colour + vec3(1.0));
    colour = pow(colour, vec3(1.0 / 2.2));

    //    FragColour = vec4(float(lightCount), float(lightCount), float(lightCount), 1.0);
    FragColour = vec4(colour, 1.0);
}