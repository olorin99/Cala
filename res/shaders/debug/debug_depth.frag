
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform texture2D textureMaps[];
layout (set = 0, binding = 2) uniform sampler samplers[];

#include "global_data.glsl"

layout (push_constant) uniform PushData {
    int depthMapIndex;
};

void main() {
    float depthValue = 1 - texture(sampler2D(textureMaps[depthMapIndex], samplers[globalData.linearRepeatSampler]), fsIn.TexCoords).r;
    FragColour = vec4(depthValue, depthValue, depthValue, 1.0);
}