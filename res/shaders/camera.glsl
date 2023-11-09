#ifndef CAMERA_DATA_GLSL
#define CAMERA_DATA_GLSL

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    float exposure;
};

//layout (set = 0, binding = 1) buffer BindlessCameraBuffer { CameraData camera; } bindlessBuffersCamera[];

layout (std430, buffer_reference, buffer_reference_align = 8) readonly buffer CameraBuffer {
    CameraData camera;
};

#endif