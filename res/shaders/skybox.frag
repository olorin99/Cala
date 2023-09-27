
layout (location = 0) in VsOut {
    vec3 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform textureCube bindlessCubeMaps[];
layout (set = 0, binding = 2) uniform sampler samplers[];

#include "global_data.glsl"

layout (push_constant) uniform PushConstants {
    int skyMapIndex;
};

void main() {
    vec3 colour = texture(samplerCube(bindlessCubeMaps[skyMapIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords).rgb;
    FragColour = vec4(colour, 1.0f);
}