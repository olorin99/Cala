vec4 evalMaterial(Material material) {

    CameraData camera = globalData.cameraBuffer.camera;

    vec3 viewPos = fsIn.ViewPos;
    vec3 V = normalize(viewPos - fsIn.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.albedo, material.metallic);
    vec3 Lo = vec3(0.0);

    uint tileIndex = getTileIndex(gl_FragCoord.xy, gl_FragCoord.z, camera.near, camera.far);

    uint lightCount = lightGridBuffer.lightGrid[tileIndex].count;
    uint lightOffset = lightGridBuffer.lightGrid[tileIndex].offset;

    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = lightIndicesBuffer.lightIndices[lightOffset + i];
        Light light = globalData.lightBuffer.lights[lightIndex];
        Lo += pointLight(light, material.normal, viewPos, V, F0, material.albedo, material.roughness, material.metallic);
    }

    vec3 ambient = getAmbient(globalData.irradianceIndex, globalData.prefilterIndex, globalData.brdfIndex, material.normal, V, F0, material.albedo, material.roughness, material.metallic);

    vec3 colour = (ambient + Lo);

    return vec4(colour, 1.0);
}