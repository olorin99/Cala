#ifndef TRANSFORMS_DATA_GLSL
#define TRANFORMS_DATA_GLSL

layout (set = 0, binding = 1) readonly buffer TransformsBuffer {
    mat4 transforms[];
} bindlessBuffersTransforms[];

#endif