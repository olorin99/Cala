#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

//#define sizeof(Type) (uint64_t(Type(uint64_t(0))+1))
#include "util.glsl"

vec4 pointLight(GPULight light, vec3 normal, vec3 viewPos, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 lightVec = light.position - fsIn.FragPos;
    vec3 L = normalize(lightVec);
    float NdotL = max(dot(normal, L), 0.0);

    float shadow = 1.0;

    if (light.shadowIndex >= 0) {
        float bias = max(light.shadowBias * (1.0 - dot(normal, L)), 0.0001);
        shadow = filterPCF(light.shadowIndex, light.position, bias, light.shadowRange);
    }

    if (shadow == 0)
    return vec4(0.0);

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

    return vec4((kD * albedo / PI + specular) * radiance * NdotL * shadow, 1.0);
}

vec4 directionalLight(GPULight light, vec3 normal, vec3 viewPos, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic) {
    vec3 L = normalize(light.position);
    float NdotL = max(dot(normal, L), 0.0);

    float shadow = 1.0;

    if (light.shadowIndex >= 0) {
        float depthValue = linearDepth(gl_FragCoord.z, globalData.cameraBuffer[globalData.primaryCameraIndex].camera.near, globalData.cameraBuffer[globalData.primaryCameraIndex].camera.far);

        for (int cascadeIndex = 0; cascadeIndex < light.cascadeCount; cascadeIndex++) {
            GPUCamera shadowCamera = globalData.cameraBuffer[light.cameraIndex + cascadeIndex].camera;
            vec4 shadowPos = shadowCamera.projection * shadowCamera.view * vec4(fsIn.FragPos, 1.0);
            vec3 shadowCoords = vec3(shadowPos.xy * 0.5 + 0.5, shadowPos.z);
            if (depthValue < light.cascades[cascadeIndex].distance &&
                light.cascades[cascadeIndex].shadowMapIndex >= 0 &&
                shadowCoords.x > 0.0 && shadowCoords.x < 1.0 &&
                shadowCoords.y > 0.0 && shadowCoords.y < 1.0) {

                float fade = 1 - (light.cascades[cascadeIndex].distance - depthValue) / light.cascades[cascadeIndex].distance;
                fade = (fade - 0.8) * 5.0;

                float shadowMain = 1.0;
                float bias = max(light.shadowBias * (1.0 - dot(normal, L)), 0.0001);
                switch (globalData.shadowMode) {
                    case 0:
                        shadowMain = pcss2D(light.cascades[cascadeIndex].shadowMapIndex, shadowCoords, light.size, bias);
                        break;
                    case 1:
                        shadowMain = filterPCF2D(light.cascades[cascadeIndex].shadowMapIndex, shadowCoords, 1, bias);
                        break;
                    case 2:
                        shadowMain = sampleShadow(light.cascades[cascadeIndex].shadowMapIndex, shadowCoords, vec2(0), bias);
                        break;
                }

                if (fade > 0 && cascadeIndex < light.cascadeCount - 1) {
                    int fadeCascadeIndex = cascadeIndex + 1;
                    GPUCamera fadeShadowCamera = globalData.cameraBuffer[light.cameraIndex + fadeCascadeIndex].camera;
                    vec4 fadeShadowPos = fadeShadowCamera.projection * fadeShadowCamera.view * vec4(fsIn.FragPos, 1.0);
                    vec3 fadeShadowCoords = vec3(fadeShadowPos.xy * 0.5 + 0.5, fadeShadowPos.z);

                    if (depthValue < light.cascades[fadeCascadeIndex].distance &&
                        light.cascades[fadeCascadeIndex].shadowMapIndex >= 0 &&
                        fadeShadowCoords.x > 0.0 && fadeShadowCoords.x < 1.0 &&
                        fadeShadowCoords.y > 0.0 && fadeShadowCoords.y < 1.0) {

                        float shadowFade = 1.0;
                        switch (globalData.shadowMode) {
                            case 0:
                                shadowFade = pcss2D(light.cascades[fadeCascadeIndex].shadowMapIndex, fadeShadowCoords, light.size, bias);
                                break;
                            case 1:
                                shadowFade = filterPCF2D(light.cascades[fadeCascadeIndex].shadowMapIndex, fadeShadowCoords, 1, bias);
                                break;
                            case 2:
                                shadowFade = sampleShadow(light.cascades[fadeCascadeIndex].shadowMapIndex, fadeShadowCoords, vec2(0), bias);
                                break;
                        }
                        shadow = mix(shadowMain, shadowFade, fade);
                    }
                } else
                    shadow = shadowMain;

                break;
            }
        }
    }

    if (shadow == 0)
        return vec4(0.0);

    vec3 H = normalize(V + L);
    vec3 radiance = light.colour * light.intensity;

    float NDF = distributionGGX(normal, H, roughness);
    float G = geometrySmith(normal, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 nominator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, V), 0.0) * NdotL + 0.0001;
    vec3 specular = nominator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    return vec4((kD * albedo / PI + specular) * radiance * NdotL * shadow, 1.0);
}

#endif