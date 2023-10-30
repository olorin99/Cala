#ifndef VXGI_GLSL
#define VXGI_GLSL

vec3 scaleAndBias(vec3 p) {
    return vec3(p.xy * 0.5 + 0.5, p.z);
}

bool isInsideCube(vec3 p, float e) {
    return p.x < 1 + e && p.y < 1 + e && p.z < 1 + e && p.x > 0 - e && p.y > 0 - e && p.z > 0 - e;
}

vec3 traceCone(vec3 origin, vec3 direction, float coneRatio) {
    vec4 colour = vec4(0.0);
    float dist = 0.1953125;
    float minDiameter = 1.0;

//    while (dist < 1 && colour.a < 0.99f) {
    for (int step = 1; step < 1000 && dist < 1 && colour.a < 0.99f; step++) {
        vec3 currentPos = origin + direction * dist;
        vec4 i = globalData.vxgi.projection * vec4(currentPos, 1.0);
        vec3 voxelPos = scaleAndBias(i.xyz);
        if (!isInsideCube(voxelPos, 0))
            continue;

        float coneDiameter = coneRatio * dist;
        float lod = clamp(log2(coneDiameter / minDiameter), 0, 1);

        vec4 voxel = textureLod(sampler3D(textureMaps3D[globalData.vxgi.gridIndex], samplers[globalData.nearestRepeatSampler]), voxelPos, lod);
        colour += (1.0f - colour.a) * voxel;
//        colour += (1.0f - colour.a) * vec4(1.0);
        dist += coneDiameter;
    }
    return colour.rgb;
}

vec3 calcDiffuse(vec3 position, vec3 normal) {
    vec3 ammount = vec3(0.0);

    ammount += traceCone(position, normal, 1.55f);

    return ammount;
}

#endif