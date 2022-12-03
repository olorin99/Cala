#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D albedoMap;
layout (set = 2, binding = 1) uniform sampler2D normalMap;
layout (set = 2, binding = 2) uniform sampler2D metallicMap;
layout (set = 2, binding = 3) uniform sampler2D roughnessMap;
layout (set = 2, binding = 4) uniform sampler2D aoMap;

struct PointLight {
    vec3 position;
    vec3 colour;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float radius;
};

layout (set = 3, binding = 0) uniform LightData {
    PointLight light;
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

void main() {

    vec3 albedo = texture(albedoMap, fsIn.TexCoords).rgb;
    vec3 normal = texture(normalMap, fsIn.TexCoords).rgb;
    float metallic = texture(metallicMap, fsIn.TexCoords).r;
    float roughness = texture(roughnessMap, fsIn.TexCoords).r;
    float ao = texture(aoMap, fsIn.TexCoords).r;

    normal = normalize(normal * 2.0 - 1.0);
    normal = normalize(fsIn.TBN * normal);

    vec3 V = normalize(fsIn.ViewPos - fsIn.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 L = normalize(light.position - fsIn.FragPos);
    vec3 H = normalize(V + L);
    float distance = length(light.position - fsIn.FragPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = light.colour * light.intensity * attenuation;


    float NDF = distributionGGX(normal, H, roughness);
    float G = geometrySmith(normal, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.001;
    vec3 specular = nominator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;

    kD *= 1.0 - metallic;

    float NdotL = max(dot(normal, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 colour = ambient + Lo;

    colour = colour / (colour + vec3(1.0));
    colour = pow(colour, vec3(1.0 / 2.2));

    FragColour = vec4(colour, 1.0);


}