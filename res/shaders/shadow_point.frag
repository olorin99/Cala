
layout (location = 0) in VsOut {
    vec3 FragPos;
} fsIn;

#include "shaderBridge.h"

layout (set = 3, binding = 0) readonly buffer LightData {
    GPULight light;
};

void main() {
    vec3 lightVec = fsIn.FragPos - light.position.xyz;
    gl_FragDepth = length(lightVec) / (light.shadowRange);
}