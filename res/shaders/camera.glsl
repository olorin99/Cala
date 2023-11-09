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

layout (set = 0, binding = 1) buffer CameraBuffer { CameraData camera; } bindlessBuffersCamera[];

#endif