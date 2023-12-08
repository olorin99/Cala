
#include "pbr.glsl"
#include "shadow.glsl"
#include "lighting.glsl"

vec4 evalMaterial(Material material) {

    GPUCamera camera = globalData.cameraBuffer.camera;

    vec3 viewPos = fsIn.ViewPos;
    vec3 V = normalize(viewPos - fsIn.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.albedo, material.metallic);
    vec3 Lo = vec3(0.0);

    uint maxTileCount = globalData.tileSizes.x * globalData.tileSizes.y * globalData.tileSizes.z - 1;
    uint tileIndex = getTileIndex(gl_FragCoord.xyz, globalData.tileSizes, globalData.swapchainSize, camera.near, camera.far);

    LightGrid grid = lightGridBuffer.lightGrid[min(tileIndex, maxTileCount)];

    for (uint i = 0; i < grid.count; i++) {
        uint lightIndex = lightIndicesBuffer.lightIndices[grid.offset + i];
        GPULight light = globalData.lightBuffer.lights[lightIndex];
        switch (light.type) {
            case 0:
                Lo += directionalLight(light, material.normal, viewPos, V, F0, material.albedo, material.roughness, material.metallic);
                break;
            case 1:
                Lo += pointLight(light, material.normal, viewPos, V, F0, material.albedo, material.roughness, material.metallic);
                break;
        }
    }

    vec3 emissive = material.albedo * material.emissive * material.emissiveStrength;
    Lo += emissive;

    vec3 ambient = getAmbient(globalData.irradianceIndex, globalData.prefilterIndex, globalData.brdfIndex, material.normal, V, F0, material.albedo, material.roughness, material.metallic);

    vec3 colour = (ambient + Lo);

    return vec4(colour, 1.0);
}
