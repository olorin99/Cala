#version 450

layout (location = 0) in VsOut {
    vec3 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform samplerCube skyMap;

void main() {
    vec3 colour = texture(skyMap, fsIn.TexCoords).rgb;
    FragColour = vec4(colour, 1.0f);
}