#ifndef TRANSFORMS_DATA_GLSL
#define TRANSFORMS_DATA_GLSL

//layout (set = 0, binding = 1) readonly buffer TransformsBuffer {
//    mat4 transforms[];
//} bindlessBuffersTransforms[];

layout (std430, buffer_reference, buffer_reference_align = 8) readonly buffer TransformsBuffer {
    mat4 transforms[];
};

#endif