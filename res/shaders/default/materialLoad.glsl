
#include "bindings.glsl"
#include "visibility_buffer/visibility.glsl"

CALA_USE_SAMPLED_IMAGE(2D)

Material loadMaterial(MaterialData data, InterpolatedValues values) {
    Material material;
    if (data.albedoIndex < 0) {
        material.albedo = vec4(1.0);
    } else {
        vec4 albedaRGBA = textureGrad(CALA_COMBINED_SAMPLER2D(nonuniformEXT(data.albedoIndex), globalData.linearRepeatSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy);
/*if (albedaRGBA.a < 0.001)\n    discard;*/
        material.albedo = albedaRGBA.rgba;
    }
    if (data.normalIndex < 0) {
        material.normal = vec3(0.52, 0.52, 1);
    } else {
        material.normal = textureGrad(CALA_COMBINED_SAMPLER2D(nonuniformEXT(data.normalIndex), globalData.linearRepeatSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).rgb;
    }
    material.normal = material.normal * 2.0 - 1.0;
    material.normal = normalize(values.TBN * material.normal);

    if (data.metallicRoughnessIndex < 0) {
        material.roughness = 1.0;
        material.metallic = 0.0;
    } else {
        material.metallic = textureGrad(CALA_COMBINED_SAMPLER2D(nonuniformEXT(data.metallicRoughnessIndex), globalData.linearRepeatSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).b;
        material.roughness = textureGrad(CALA_COMBINED_SAMPLER2D(nonuniformEXT(data.metallicRoughnessIndex), globalData.linearRepeatSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).g;
    }

    if (data.emissiveIndex < 0) {
        material.emissive = vec3(1);
    } else {
        material.emissive = textureGrad(CALA_COMBINED_SAMPLER2D(nonuniformEXT(data.emissiveIndex), globalData.linearRepeatSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).rgb;
    }

    material.emissiveStrength = data.emissiveStrength;
    return material;
}
