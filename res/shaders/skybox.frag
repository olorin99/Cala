
layout (location = 0) in VsOut {
    vec3 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

#include "shaderBridge.h"
#include "bindings.glsl"

CALA_USE_SAMPLED_IMAGE(Cube)

layout (push_constant) uniform PushConstants {
    int skyMapIndex;
};

void main() {
    vec3 colour = texture(CALA_COMBINED_SAMPLER(Cube, skyMapIndex, globalData.linearRepeatSampler), fsIn.TexCoords).rgb;
    FragColour = vec4(colour, 1.0f);
}