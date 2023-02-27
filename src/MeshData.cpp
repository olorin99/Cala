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

cala::backend::vulkan::Buffer cala::MeshData::vertexBuffer(backend::vulkan::Device& driver) const {
    backend::vulkan::Buffer buf(driver, _vertices.size() * sizeof(Vertex), backend::BufferUsage::VERTEX, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);
    buf.data({_vertices.data(), static_cast<u32>(_vertices.size() * sizeof(Vertex))});
    return std::move(buf);
}

cala::backend::vulkan::Buffer cala::MeshData::indexBuffer(backend::vulkan::Device& driver) const {
    backend::vulkan::Buffer buf(driver, _indices.size() * sizeof(u32), backend::BufferUsage::INDEX, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);
    buf.data({_indices.data(), static_cast<u32>(_indices.size() * sizeof(u32))});
    return std::move(buf);
}

cala::Mesh cala::MeshData::mesh(cala::Engine* engine) {
//    auto vertex = vertexBuffer(device);
//    auto index = indexBuffer(device);

//    auto vertex = engine->createBuffer(_vertices.size() * sizeof(Vertex), backend::BufferUsage::VERTEX);
//    auto index = engine->createBuffer(_indices.size() * sizeof(u32), backend::BufferUsage::INDEX);

//    vertex->data({_vertices.data(), static_cast<u32>(_vertices.size() * sizeof(Vertex))});
//    index->data({_indices.data(), static_cast<u32>(_indices.size() * sizeof(u32))});

    u32 vertexOffset = engine->uploadVertexData({ reinterpret_cast<f32*>(_vertices.data()), static_cast<u32>(_vertices.size() * sizeof(Vertex)) });
    for (auto& index : _indices)
        index += vertexOffset / sizeof(Vertex);
    u32 indexOffset = engine->uploadIndexData({ reinterpret_cast<u32*>(_indices.data()), static_cast<u32>(_indices.size() * sizeof(u32)) });

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

    Mesh mesh = {{}, {}, static_cast<u32>(indexOffset / sizeof(u32)), (u32)_indices.size(), binding, attributes};
    return std::move(mesh);
}