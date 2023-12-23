#ifndef CALA_MODEL_H
#define CALA_MODEL_H

#include <Ende/math/Vec.h>
#include <Cala/vulkan/Handle.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Transform.h>
#include <string>

namespace cala {

    class Model {
    public:

        Model() = default;

        struct AABB {
            ende::math::Vec3f min;
            ende::math::Vec3f max;
        };

        struct LOD {
            u32 meshletOffset;
            u32 meshletCount;
        };

        struct Primitive {
            u32 firstIndex;
            u32 indexCount;
            u32 meshletIndex;
            u32 meshletCount;
            i32 materialIndex;
            AABB aabb;
            LOD lods[10];
            u32 lodCount;
        };

        struct Node {
            std::vector<u32> primitives;
            std::vector<u32> children;
            Transform transform;
            std::string name;
        };

        std::vector<Node> nodes;

        std::vector<Primitive> primitives;
        std::vector<MaterialInstance> materials;

        std::vector<vk::ImageHandle> images;

        VkVertexInputBindingDescription _binding = {};
        std::array<vk::Attribute, 4> _attributes = {};

    };

}

#endif //CALA_MODEL_H
