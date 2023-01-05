#version 450

layout (location = 0) in VsOut {
    vec3 FragPos;
} fsIn;

struct PointLight {
    vec3 position;
    vec3 colour;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float radius;
};

layout (set = 3, binding = 0) readonly buffer LightData {
    PointLight light;
};

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
};

layout (push_constant) uniform PushConstants {
    CameraData camera;
};

void main() {
    vec3 lightVec = fsIn.FragPos - light.position;
    gl_FragDepth = length(lightVec) / (camera.far - camera.near);
}