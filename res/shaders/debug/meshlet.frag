
layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
    flat uint meshletID;
} fsIn;

layout (location = 0) out vec4 FragColour;

#include "util.glsl"

vec3 hue2rgb(float hue) {
    hue = fract(hue);
    float r = abs(hue * 6 - 3) - 1;
    float g = 2 - abs(hue * 6 - 2);
    float b = 2 - abs(hue * 6 - 4);
    return clamp(vec3(r, g, b), vec3(0), vec3(1));
}

void main() {
//    vec3 colour = vec3(random(float(fsIn.meshletID)), random(float(fsIn.meshletID) * 2), random(float(fsIn.meshletID) / 2));
    vec3 colour = hue2rgb(fsIn.meshletID * 1.71f);
    FragColour = vec4(colour, 1.0);
}
