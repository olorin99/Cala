//
// Created by christian on 22/08/22.
//

#ifndef CALA_MESH_H
#define CALA_MESH_H

#include <Cala/backend/vulkan/Buffer.h>

#include <optional>

namespace cala {

    class Mesh {
    public:

        Mesh(backend::vulkan::Buffer&& vertex, std::optional<backend::vulkan::Buffer> index, VkVertexInputBindingDescription binding, std::array<backend::Attribute, 5> attributes);

    //private:

        backend::vulkan::Buffer _vertex;
        std::optional<backend::vulkan::Buffer> _index; // optional

        VkVertexInputBindingDescription _binding;
        std::array<backend::Attribute, 5> _attributes;

    };


}

#endif //CALA_MESH_H
