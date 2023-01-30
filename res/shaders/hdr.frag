#version 450

layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    float exposure;
};

layout (set = 1, binding = 0) uniform FrameData {
    CameraData camera;
};

layout (set = 2, binding = 0) uniform sampler2D hdrMap;

void main() {
    const float gamma = 2.2;

    vec3 hdr = texture(hdrMap, fsIn.TexCoords).rgb;
    vec3 result = vec3(1.0) - exp(-hdr * camera.exposure);
    result = pow(result, vec3(1.0 / gamma));
    FragColour = vec4(result, 1.0);
}