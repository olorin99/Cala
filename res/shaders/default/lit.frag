
#include "pbr.glsl"
#include "shadow.glsl"
#include "lighting.glsl"

vec4 evalMaterial(Material material) {
//    float opacity = material.albedo.a;
//    int x = int(gl_FragCoord.x) % 4;
//    int y = int(gl_FragCoord.y) % 4;
//    int index = x + y * 4;
//    float limit = 0;
//    if (x < 8) {
//        if (index == 0) limit = 0.0625;
//        if (index == 1) limit = 0.5625;
//        if (index == 2) limit = 0.1875;
//        if (index == 3) limit = 0.6875;
//        if (index == 4) limit = 0.8125;
//        if (index == 5) limit = 0.3125;
//        if (index == 6) limit = 0.9375;
//        if (index == 7) limit = 0.4375;
//        if (index == 8) limit = 0.25;
//        if (index == 9) limit = 0.75;
//        if (index == 10) limit = 0.126;
//        if (index == 11) limit = 0.625;
//        if (index == 12) limit = 1.0;
//        if (index == 13) limit = 0.5;
//        if (index == 14) limit = 0.875;
//        if (index == 15) limit = 0.375;
//    }
////    return vec4(opacity, limit, 0, 0);
//    if (opacity < limit)
//        discard;

    GPUCamera camera = globalData.cameraBuffer[globalData.primaryCameraIndex].camera;

    vec3 viewPos = fsIn.ViewPos;
    vec3 V = normalize(viewPos - fsIn.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.albedo.rgb, material.metallic);
    vec4 Lo = vec4(0.0);

    uint maxTileCount = globalData.tileSizes.x * globalData.tileSizes.y * globalData.tileSizes.z - 1;
    uint tileIndex = getTileIndex(gl_FragCoord.xyz, globalData.tileSizes, globalData.swapchainSize, camera.near, camera.far);

    LightGrid grid = lightGridBuffer.lightGrid[min(tileIndex, maxTileCount)];

    for (uint i = 0; i < grid.count; i++) {
        uint lightIndex = lightIndicesBuffer.lightIndices[grid.offset + i];
        GPULight light = globalData.lightBuffer.lights[lightIndex];
        switch (light.type) {
            case 0:
                Lo += directionalLight(light, material.normal, viewPos, V, F0, material.albedo.rgb, material.roughness, material.metallic);
                break;
            case 1:
                Lo += pointLight(light, material.normal, viewPos, V, F0, material.albedo.rgb, material.roughness, material.metallic);
                break;
        }
    }

    vec3 emissive = material.albedo.rgb * material.emissive * material.emissiveStrength;
    Lo += vec4(emissive, 0);

    vec3 ambient = getAmbient(globalData.irradianceIndex, globalData.prefilterIndex, globalData.brdfIndex, material.normal, V, F0, material.albedo.rgb, material.roughness, material.metallic);

    vec4 colour = (vec4(ambient, 0.0) + Lo);
    return colour;
//    return vec4(colour, 1.0);
}
