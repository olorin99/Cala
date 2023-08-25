#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform samplerCube cubeMaps[];
layout (set = 0, binding = 0) uniform sampler2D textureMaps[];


layout (push_constant) uniform PushData {
    vec4 colour;
};

void main() {
    FragColour = colour;
}