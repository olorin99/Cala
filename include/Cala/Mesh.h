#ifndef CALA_MESH_H
#define CALA_MESH_H

#include <Cala/vulkan/Buffer.h>
#include <Cala/Engine.h>
#include <optional>
#include <Cala/shaderBridge.h>

namespace cala {

    struct Mesh {
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 meshletIndex = 0;
        u32 meshletCount = 0;
        MaterialInstance* materialInstance = nullptr;
        ende::math::Vec4f min = { -1, -1, -1 };
        ende::math::Vec4f max = { 1, 1, 1 };
        LOD lods[MAX_LODS] = {};
        u32 lodCount = 0;

        VkVertexInputBindingDescription _binding = {};
        std::array<vk::Attribute, 3> _attributes = {};

    };

}

#endif //CALA_MESH_H
