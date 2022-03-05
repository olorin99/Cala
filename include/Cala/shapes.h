#ifndef CALA_SHAPES_H
#define CALA_SHAPES_H

#include <Cala/Mesh.h>
#include <Ende/math/Mat.h>

namespace cala::shapes {

    Mesh triangle(f32 width = 1.f, f32 height = 1.f);

    Mesh quad(f32 edge = 1.f);

    Mesh cube(f32 edge = 1.f);

    Mesh sphereUV(f32 radius = 0.5f, u32 rings = 16, u32 sectors = 16);

    Mesh sphereNormalized(f32 radius = 0.5f, u32 divisions = 6);

    Mesh sphereCube(f32 radius = 0.5f, u32 divisions = 6);

    Mesh icosahedron(f32 radius = 0.5);

    Mesh frustum(const ende::math::Mat4f& matrix);

}

#endif //CALA_SHAPES_H