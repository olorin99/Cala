#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 2, binding = 0) uniform sampler2D metalMap;

void main() {
//    FragColour = vec4(1, 1, 1, 1);
    FragColour = vec4(texture(metalMap, fsIn.TexCoords).xyz, 1.0f);
}