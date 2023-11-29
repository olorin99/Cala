#include "Cala/MeshData.h"

bool cala::operator==(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position == rhs.position ||
            lhs.normal == rhs.normal ||
            lhs.texCoords == rhs.texCoords ||
            lhs.tangent == rhs.tangent;
//            lhs.bitangent == rhs.bitangent;
}

bool cala::operator!=(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position != rhs.position ||
            lhs.normal != rhs.normal ||
            lhs.texCoords != rhs.texCoords ||
            lhs.tangent != rhs.tangent;
//            lhs.bitangent != rhs.bitangent;
}



cala::MeshData &cala::MeshData::addVertex(const Vertex &vertex) {
    _vertices.push_back(vertex);
    return *this;
}

cala::MeshData &cala::MeshData::addIndex(u32 index) {
    _indices.push_back(index);
    return *this;
}

cala::MeshData &cala::MeshData::addTriangle(u32 a, u32 b, u32 c) {
    _indices.push_back(a);
    _indices.push_back(b);
    _indices.push_back(c);
    return *this;
}

cala::MeshData &cala::MeshData::addQuad(u32 a, u32 b, u32 c, u32 d) {
    addTriangle(a, b, d);
    addTriangle(d, c, a);
    return *this;
}

cala::backend::vulkan::BufferHandle cala::MeshData::vertexBuffer(backend::vulkan::Device& driver) const {
    auto buf = driver.createBuffer(_vertices.size() * sizeof(Vertex), backend::BufferUsage::VERTEX, backend::MemoryProperties::STAGING);
//    buf->data({_vertices.data(), static_cast<u32>(_vertices.size() * sizeof(Vertex))});
//    buf->data(ende::Span<Vertex>{_vertices.data(), _vertices.size() });
    std::span<const Vertex> vs = _vertices;
    buf->data(vs);
    return buf;
}

cala::backend::vulkan::BufferHandle cala::MeshData::indexBuffer(backend::vulkan::Device& driver) const {
    auto buf = driver.createBuffer(_indices.size() * sizeof(u32), backend::BufferUsage::INDEX, backend::MemoryProperties::STAGING);
//    buf->data({_indices.data(), static_cast<u32>(_indices.size() * sizeof(u32))});
    std::span<const u32> is = _indices;
    buf->data(is);
    return buf;
}

cala::Mesh cala::MeshData::mesh(cala::Engine* engine) {

//    u32 vertexOffset = engine->uploadVertexData({ reinterpret_cast<f32*>(_vertices.data()), static_cast<u32>(_vertices.size() * sizeof(Vertex)) });
    u32 vertexOffset = engine->uploadVertexData({ reinterpret_cast<f32*>(_vertices.data()), static_cast<u32>(_vertices.size() * sizeof(Vertex) / sizeof(f32)) });
    for (auto& index : _indices)
        index += vertexOffset / sizeof(Vertex);
    u32 indexOffset = engine->uploadIndexData(_indices);

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 12 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<backend::Attribute, 4> attributes = {
            backend::Attribute{0, 0, backend::AttribType::Vec3f},
            backend::Attribute{1, 0, backend::AttribType::Vec3f},
            backend::Attribute{2, 0, backend::AttribType::Vec2f},
            backend::Attribute{3, 0, backend::AttribType::Vec4f}
    };

    Mesh mesh = {static_cast<u32>(indexOffset / sizeof(u32)), (u32)_indices.size(), {}, {}, binding, attributes};
    return std::move(mesh);
}