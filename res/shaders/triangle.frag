#version 450

layout (location = 0) in vec3 fragColour;

layout (location = 0) out vec4 outColour;

layout (set = 0, binding = 0) uniform Colour {
    vec3 colour;
};

void main() {
    outColour = vec4(fragColour * colour, 1.0);
}