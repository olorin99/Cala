#ifndef CALA_MODEL_H
#define CALA_MODEL_H

#include <Ende/math/Vec.h>
#include <Cala/backend/vulkan/Handle.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Transform.h>

namespace cala {

    class Model {
    public:

        Model() = default;

        struct AABB {
            ende::math::Vec3f min;
            ende::math::Vec3f max;
        };

        struct Primitive {
            u32 firstIndex;
            u32 indexCount;
            i32 materialIndex;
            AABB aabb;
        };

        struct Node {
            std::vector<u32> primitives;
            std::vector<u32> children;
            Transform transform;
        };

        std::vector<Node> nodes;

        std::vector<Primitive> primitives;
        std::vector<MaterialInstance> materials;

        std::vector<backend::vulkan::ImageHandle> images;

        VkVertexInputBindingDescription _binding = {};
        std::array<backend::Attribute, 4> _attributes = {};

    };

}

#endif //CALA_MODEL_H
