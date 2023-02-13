#ifndef CALA_MESH_H
#define CALA_MESH_H

#include <Cala/backend/vulkan/Buffer.h>
#include <Ende/Vector.h>
#include <Cala/Engine.h>
#include <optional>

namespace cala {

    struct Mesh {

        BufferHandle _vertex;
        BufferHandle _index; // optional

        u32 firstIndex = 0;
        u32 indexCount = 0;

        VkVertexInputBindingDescription _binding;
        std::array<backend::Attribute, 4> _attributes;

    };

}

#endif //CALA_MESH_H
