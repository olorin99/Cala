
layout (location = 0) in VsOut {
    vec3 FragPos;
} fsIn;

struct Light {
    vec4 position;
    vec3 colour;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    //    float radius;
    uint shadowIndex;
};

layout (set = 3, binding = 0) readonly buffer LightData {
    Light light;
};

struct CameraData {
    mat4 viewProjection;
    vec3 position;
    float near;
    float far;
};

layout (push_constant) uniform PushConstants {
    CameraData camera;
};

void main() {
    vec3 lightVec = fsIn.FragPos - light.position.xyz;
    gl_FragDepth = length(lightVec) / (camera.far - camera.near);
}