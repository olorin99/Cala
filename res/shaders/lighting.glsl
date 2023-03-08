struct Light {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float shadowRange;
    float radius;
    float shadowBias;
    int shadowIndex;
};

vec3 pointLight(Light light, vec3 normal, vec3 viewPos, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 lightVec = light.position - fsIn.FragPos;
    vec3 L = normalize(lightVec);
    float NdotL = max(dot(normal, L), 0.0);

    float shadow = 1.0;

    if (light.shadowIndex >= 0) {
        float bias = max(light.shadowBias * (1.0 - dot(normal, L)), 0.0001);
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
    //    return vec3(NdotL);
    //    return L;
    return (kD * albedo / PI + specular) * radiance * NdotL * shadow;
}