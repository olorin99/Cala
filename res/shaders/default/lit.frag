vec4 evalMaterial(Material material) {

    CameraData camera = bindlessBuffersCamera[globalData.cameraBufferIndex].camera;

    vec3 viewPos = fsIn.ViewPos;
    vec3 V = normalize(viewPos - fsIn.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.albedo, material.metallic);
    vec3 Lo = vec3(0.0);

    uint tileIndex = getTileIndex(gl_FragCoord.xy, gl_FragCoord.z, camera.near, camera.far);

    uint lightCount = bindlessBuffersLightGrid[lightGridIndex].lightGrid[tileIndex].count;
    uint lightOffset = bindlessBuffersLightGrid[lightGridIndex].lightGrid[tileIndex].offset;

    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = bindlessBuffersLightIndices[lightIndicesIndex].lightIndices[lightOffset + i];
        Light light = bindlessBuffersLights[globalData.lightBufferIndex].lights[lightIndex];
        Lo += pointLight(light, material.normal, viewPos, V, F0, material.albedo, material.roughness, material.metallic);
    }

    vec3 ambient = getAmbient(globalData.irradianceIndex, globalData.prefilterIndex, globalData.brdfIndex, material.normal, V, F0, material.albedo, material.roughness, material.metallic);

    if (globalData.vxgi.gridIndex >= 0) {
        ambient += traceCone(fsIn.FragPos + material.normal * globalData.vxgi.voxelExtent.x, material.normal, 1.55f);
    }

    vec3 colour = (ambient + Lo);

    return vec4(colour, 1.0);
}