
#include "bindings.glsl"

CALA_USE_SAMPLED_IMAGE(2D)

Material loadMaterial(MaterialData data) {
    Material material;
    if (data.albedoIndex < 0) {
        material.albedo = vec4(1.0);
    } else {
        vec4 albedaRGBA = texture(CALA_COMBINED_SAMPLER2D(data.albedoIndex, globalData.linearRepeatSampler), fsIn.TexCoords);
/*if (albedaRGBA.a < 0.001)\n    discard;*/
        material.albedo = albedaRGBA.rgba;
    }
    if (data.normalIndex < 0) {
        material.normal = vec3(0.52, 0.52, 1);
    } else {
        material.normal = texture(CALA_COMBINED_SAMPLER2D(data.normalIndex, globalData.linearRepeatSampler), fsIn.TexCoords).rgb;
    }
    material.normal = material.normal * 2.0 - 1.0;
    material.normal = normalize(fsIn.TBN * material.normal);

    if (data.metallicRoughnessIndex < 0) {
        material.roughness = 1.0;
        material.metallic = 0.0;
    } else {
        material.metallic = texture(CALA_COMBINED_SAMPLER2D(data.metallicRoughnessIndex, globalData.linearRepeatSampler), fsIn.TexCoords).b;
        material.roughness = texture(CALA_COMBINED_SAMPLER2D(data.metallicRoughnessIndex, globalData.linearRepeatSampler), fsIn.TexCoords).g;
    }

    if (data.emissiveIndex < 0) {
        material.emissive = vec3(1);
    } else {
        material.emissive = texture(CALA_COMBINED_SAMPLER2D(data.emissiveIndex, globalData.linearRepeatSampler), fsIn.TexCoords).rgb;
    }

    material.emissiveStrength = data.emissiveStrength;
    return material;
}
