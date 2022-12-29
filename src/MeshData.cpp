#include "Cala/MeshData.h"

bool cala::operator==(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position == rhs.position ||
            lhs.normal == rhs.normal ||
            lhs.texCoords == rhs.texCoords ||
            lhs.tangent == rhs.tangent ||
            lhs.bitangent == rhs.bitangent;
}

bool cala::operator!=(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position != rhs.position ||
            lhs.normal != rhs.normal ||
            lhs.texCoords != rhs.texCoords ||
            lhs.tangent != rhs.tangent ||
            lhs.bitangent != rhs.bitangent;
}



cala::MeshData &cala::MeshData::addVertex(const Vertex &vertex) {
    _vertices.push(vertex);
    return *this;
}

cala::MeshData &cala::MeshData::addIndex(u32 index) {
    _indices.push(index);
    return *this;
}

cala::MeshData &cala::MeshData::addTriangle(u32 a, u32 b, u32 c) {
    _indices.push(a);
    _indices.push(b);
    _indices.push(c);
    return *this;
}

cala::MeshData &cala::MeshData::addQuad(u32 a, u32 b, u32 c, u32 d) {
    addTriangle(a, b, d);
    addTriangle(d, c, a);
    return *this;
}

cala::backend::vulkan::Buffer cala::MeshData::vertexBuffer(backend::vulkan::Driver& driver) const {
    backend::vulkan::Buffer buf(driver, _vertices.size() * sizeof(Vertex), backend::BufferUsage::VERTEX, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);
    buf.data({_vertices.data(), static_cast<u32>(_vertices.size() * sizeof(Vertex))});
    return std::move(buf);
}

cala::backend::vulkan::Buffer cala::MeshData::indexBuffer(backend::vulkan::Driver& driver) const {
    backend::vulkan::Buffer buf(driver, _indices.size() * sizeof(u32), backend::BufferUsage::INDEX, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);
    buf.data({_indices.data(), static_cast<u32>(_indices.size() * sizeof(u32))});
    return std::move(buf);
}

cala::Mesh cala::MeshData::mesh(backend::vulkan::Driver &driver) const {
    auto vertex = vertexBuffer(driver);
    auto index = indexBuffer(driver);

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<backend::Attribute, 5> attributes = {
            backend::Attribute{0, 0, backend::AttribType::Vec3f},
            backend::Attribute{1, 0, backend::AttribType::Vec3f},
            backend::Attribute{2, 0, backend::AttribType::Vec2f},
            backend::Attribute{3, 0, backend::AttribType::Vec3f},
            backend::Attribute{4, 0, backend::AttribType::Vec3f}
    };

    return {std::move(vertex), std::move(index), binding, attributes};
}