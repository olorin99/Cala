
Material loadMaterial(MaterialData data) {
    Material material;
    if (data.albedoIndex < 0) {
        material.albedo = vec3(1.0);
    } else {
        vec4 albedaRGBA = texture(sampler2D(textureMaps[data.albedoIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords);
/*if (albedaRGBA.a < 0.001)\n    discard;*/
        material.albedo = albedaRGBA.rgb;
    }
    if (data.normalIndex < 0) {
        material.normal = vec3(0.52, 0.52, 1);
    } else {
        material.normal = texture(sampler2D(textureMaps[data.normalIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords).rgb;
    }
    material.normal = material.normal * 2.0 - 1.0;
    material.normal = normalize(fsIn.TBN * material.normal);

    if (data.metallicRoughnessIndex < 0) {
        material.roughness = 1.0;
        material.metallic = 0.0;
    } else {
        material.metallic = texture(sampler2D(textureMaps[data.metallicRoughnessIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords).b;
        material.roughness = texture(sampler2D(textureMaps[data.metallicRoughnessIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords).g;
    }

    if (data.emissiveIndex < 0) {
        material.emissive = vec3(1);
    } else {
        material.emissive = texture(sampler2D(textureMaps[data.emissiveIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords).rgb;
    }

    material.emissiveStrength = data.emissiveStrength;
    return material;
}
