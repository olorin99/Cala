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

        std::vector<Primitive> primitives;
        std::vector<MaterialInstance> materials;

        backend::vulkan::BufferHandle vertexBuffer;
        backend::vulkan::BufferHandle indexBuffer;
        std::vector<backend::vulkan::ImageHandle> images;

        VkVertexInputBindingDescription _binding;
        std::array<backend::Attribute, 4> _attributes;

    };

}

#endif //CALA_MODEL_H
