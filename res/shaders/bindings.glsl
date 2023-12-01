#ifndef SHADER_BINDINGS_GLSL
#define SHADER_BINDINGS_GLSL

#include "shaderBridge.h"

layout (set = 0, binding = CALA_BINDLESS_SAMPLERS) uniform sampler calaBindlessSamplers[];

#define CALA_USE_SAMPLED_IMAGE(dimension) layout (set = 0, binding = CALA_BINDLESS_SAMPLED_IMAGE) uniform texture##dimension calaBindlessSampledImages##dimension[];
#define CALA_USE_STORAGE_IMAGE(dimension, annotation) layout (set = 0, binding = CALA_BINDLESS_STORAGE_IMAGE) uniform annotation image##dimension calaBindlessStorageImages##dimension##annotation[];
#define CALA_USE_STORAGE_IMAGE_FORMAT(dimension, annotation, format) layout (format, set = 0, binding = CALA_BINDLESS_STORAGE_IMAGE) uniform annotation image##dimension calaBindlessStorageImages##dimension##annotation##format[];

#define CALA_GET_SAMPLED_IMAGE(dimension, index) calaBindlessSampledImages##dimension[index]

#define CALA_GET_STORAGE_IMAGE(dimension, annotation, index) calaBindlessStorageImages##dimension##annotation[index]
#define CALA_GET_STORAGE_IMAGE_FORMAT(dimension, annotation, format, index) calaBindlessStorageImages##dimension##annotation##format[index]

#define CALA_COMBINED_SAMPLER(dimension, imageIndex, samplerIndex) sampler##dimension(calaBindlessSampledImages##dimension[imageIndex], calaBindlessSamplers[samplerIndex])

#define CALA_GET_SAMPLED_IMAGE2D(index) calaBindlessSampledImages2D[index]
#define CALA_GET_STORAGE_IMAGE2D(annotation, index) calaBindlessStorageImages2D##annotation[index]
#define CALA_COMBINED_SAMPLER2D(imageIndex, samplerIndex) sampler2D(calaBindlessSampledImages2D[imageIndex], calaBindlessSamplers[samplerIndex])

#define CALA_GET_SAMPLED_IMAGE3D(index) calaBindlessSampledImages3D[index]
#define CALA_GET_STORAGE_IMAGE3D(annotation, index) calaBindlessStorageImages3D##annotation[index]
#define CALA_COMBINED_SAMPLER3D(imageIndex, samplerIndex) sampler2D(calaBindlessSampledImages3D[imageIndex], calaBindlessSamplers[samplerIndex])

#define CALA_COMBINED_SAMPLERCUBE(imageIndex, samplerIndex) samplerCube(calaBindlessSampledImagesCube[imageIndex], calaBindlessSamplers[samplerIndex])

#endif