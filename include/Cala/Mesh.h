#ifndef CALA_MESH_H
#define CALA_MESH_H

#include <Cala/vulkan/Buffer.h>
#include <Cala/Engine.h>
#include <optional>

namespace cala {

    struct Mesh {
        u32 firstIndex = 0;
        u32 indexCount = 0;
        MaterialInstance* materialInstance = nullptr;
        ende::math::Vec4f min = { -1, -1, -1 };
        ende::math::Vec4f max = { 1, 1, 1 };

        VkVertexInputBindingDescription _binding = {};
        std::array<vk::Attribute, 4> _attributes = {};

    };

}

#endif //CALA_MESH_H
