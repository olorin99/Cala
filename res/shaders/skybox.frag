
layout (location = 0) in VsOut {
    vec3 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform samplerCube bindlessCubeMaps[];

layout (push_constant) uniform PushConstants {
    int skyMapIndex;
};

void main() {
    vec3 colour = texture(bindlessCubeMaps[skyMapIndex], fsIn.TexCoords).rgb;
    FragColour = vec4(colour, 1.0f);
}