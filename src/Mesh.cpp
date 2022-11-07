#include "Cala/Mesh.h"

cala::Mesh::Mesh(backend::vulkan::Buffer &&vertex, std::optional<backend::vulkan::Buffer> index, VkVertexInputBindingDescription binding, std::array<backend::Attribute, 5> attributes)
    : _vertex(std::forward<backend::vulkan::Buffer>(vertex)),
      _index(std::move(index)),
      _binding(binding),
      _attributes(attributes)
{

}