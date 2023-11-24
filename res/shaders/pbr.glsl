#ifndef PBR_GLSL
#define PBR_GLSL

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
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 getAmbient(int irradianceIndex, int prefilterIndex, int brdfIndex, vec3 normal, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 R = reflect(-V, normal);
    vec3 F = fresnelSchlickRoughness(max(dot(normal, V), 0.0), F0, roughness);
    vec3 kD = vec3(1.0) - F;
    kD *= 1.0 - metallic;
    vec3 diffuse = vec3(0.0001) * albedo;
    vec3 specular = vec3(0.0);
    if (irradianceIndex >= 0) {
        vec3 irradiance = texture(samplerCube(cubeMaps[irradianceIndex] , samplers[globalData.linearRepeatSampler]), normal).rgb;
        diffuse = irradiance * albedo;
    }
    if (prefilterIndex >= 0 && brdfIndex >= 0) {
        const float MAX_REFLECTION_LOD = 9.0;
        float lod = roughness * MAX_REFLECTION_LOD;
        float lodf = floor(lod);
        float lodc = clamp(ceil(lod), 0.0, MAX_REFLECTION_LOD);
        vec3 a = textureLod(samplerCube(cubeMaps[prefilterIndex], samplers[globalData.linearRepeatSampler]), R, lodf).rgb;
        vec3 b = textureLod(samplerCube(cubeMaps[prefilterIndex], samplers[globalData.linearRepeatSampler]), R, lodc).rgb;
        vec3 prefilteredColour = mix(a, b, lod - lodf);

        vec2 brdf = texture(sampler2D(textureMaps[brdfIndex], samplers[globalData.linearRepeatSampler]), vec2(max(dot(normal, V), 0.0), roughness)).rg;
        specular = prefilteredColour * (F * brdf.x + brdf.y);
    }
    return kD * diffuse + specular;
}

#endif