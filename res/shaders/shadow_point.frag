#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
} fsIn;

//layout (location = 0) out float FragColour;

struct PointLight {
    vec3 position;
    vec3 colour;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float radius;
};

layout (set = 3, binding = 0) uniform LightData {
    PointLight light;
};

void main() {
    vec3 lightVec = fsIn.FragPos - light.position;
    gl_FragDepth = length(lightVec) / 100.f;
}