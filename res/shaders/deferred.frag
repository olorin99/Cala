#version 450

layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform sampler2D gAlbedoMap;
layout (set = 0, binding = 1) uniform sampler2D gNormalMap;
layout (set = 0, binding = 2) uniform sampler2D gDepthMap;

void main() {
    //    FragColour = vec4(1, 1, 1, 1);
    FragColour = vec4(texture(gAlbedoMap, gl_FragCoord.xy / vec2(800, 600)).xyz, 1.0f);
}