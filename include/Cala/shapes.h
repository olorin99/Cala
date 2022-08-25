#ifndef CALA_SHAPES_H
#define CALA_SHAPES_H

#include <Cala/MeshData.h>
#include <Ende/math/Mat.h>

namespace cala::shapes {

    MeshData triangle(f32 width = 1.f, f32 height = 1.f);

    MeshData quad(f32 edge = 1.f);

    MeshData cube(f32 edge = 1.f);

    MeshData sphereUV(f32 radius = 0.5f, u32 rings = 16, u32 sectors = 16);

    MeshData sphereNormalized(f32 radius = 0.5f, u32 divisions = 6);

    MeshData sphereCube(f32 radius = 0.5f, u32 divisions = 6);

    MeshData icosahedron(f32 radius = 0.5);

    MeshData frustum(const ende::math::Mat4f& matrix);

}

#endif //CALA_SHAPES_H