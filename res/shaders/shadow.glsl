float calcShadows(uint index, vec3 viewDir, vec3 offset, float bias, float range) {
    float shadow = 1.0;
    if(texture(cubeMaps[index], viewDir + offset).r < length(viewDir) / range - bias) {
        shadow = 0.0;
    }
    return shadow;
}

vec3 sampleOffsetDirections[20] = vec3[](
vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

float filterPCF(uint index, vec3 lightPos, float bias, float range) {
    float shadow = 0;
    int samples = 20;
    vec3 viewDir = fsIn.FragPos - lightPos;
    float viewDistance = length(viewDir);
    float diskRadius = (1.0 + (viewDistance / 100.f)) / 25.0;

    for (int i = 0; i < samples; i++) {
        shadow += calcShadows(index, viewDir, sampleOffsetDirections[i] * diskRadius, bias, range);
    }
    return shadow / samples;
}