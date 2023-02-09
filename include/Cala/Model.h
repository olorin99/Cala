//
// Created by christian on 2/4/23.
//

#ifndef CALA_MODEL_H
#define CALA_MODEL_H

#include <Cala/Mesh.h>
#include <Ende/math/Vec.h>

namespace cala {

    class Model {
    public:

        struct AABB {
            ende::math::Vec3f min;
            ende::math::Vec3f max;
        };

        struct Primitive {
            u32 firstIndex;
            u32 indexCount;
            u32 materialIndex;
            AABB aabb;
        };

        ende::Vector<Primitive> primitives;
        ende::Vector<MaterialInstance> materials;

        BufferHandle vertexBuffer;
        BufferHandle indexBuffer;
        ende::Vector<ImageHandle> images;

        VkVertexInputBindingDescription _binding;
        std::array<backend::Attribute, 4> _attributes;

    };

}

#endif //CALA_MODEL_H
