
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

#include "shaderBridge.h"
#include "bindings.glsl"

CALA_USE_SAMPLED_IMAGE(2D)

layout (push_constant) uniform PushData {
    int depthMapIndex;
};

void main() {
    float depthValue = 1 - texture(CALA_COMBINED_SAMPLER2D(depthMapIndex, globalData.linearRepeatSampler), fsIn.TexCoords).r;
    FragColour = vec4(depthValue, depthValue, depthValue, 1.0);
}