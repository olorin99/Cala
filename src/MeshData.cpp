#include "Cala/MeshData.h"

bool cala::operator==(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position == rhs.position ||
            lhs.normal == rhs.normal ||
            lhs.texCoords == rhs.texCoords;
//            lhs.bitangent == rhs.bitangent;
}

bool cala::operator!=(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position != rhs.position ||
            lhs.normal != rhs.normal ||
            lhs.texCoords != rhs.texCoords;
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

cala::vk::BufferHandle cala::MeshData::vertexBuffer(vk::Device& driver) const {
    auto buf = driver.createBuffer({
        .size = (u32)(_vertices.size() * sizeof(Vertex)),
        .usage = vk::BufferUsage::VERTEX,
        .memoryType = vk::MemoryProperties::STAGING});
    std::span<const Vertex> vs = _vertices;
    buf->data(vs);
    return buf;
}

cala::vk::BufferHandle cala::MeshData::indexBuffer(vk::Device& driver) const {
    auto buf = driver.createBuffer({
        .size = (u32)(_indices.size() * sizeof(u32)),
        .usage = vk::BufferUsage::INDEX,
        .memoryType = vk::MemoryProperties::STAGING
    });
    std::span<const u32> is = _indices;
    buf->data(is);
    return buf;
}

cala::Mesh cala::MeshData::mesh(cala::Engine* engine) {

    u32 vertexOffset = engine->uploadVertexData({ reinterpret_cast<f32*>(_vertices.data()), static_cast<u32>(_vertices.size() * sizeof(Vertex) / sizeof(f32)) });
    for (auto& index : _indices)
        index += vertexOffset / sizeof(Vertex);
    u32 indexOffset = engine->uploadIndexData(_indices);

    Mesh mesh = {static_cast<u32>(indexOffset / sizeof(u32)), (u32)_indices.size(), 0, 0, nullptr, {}, {}, {}, 0, engine->globalBinding(), engine->globalVertexAttributes()};
    return std::move(mesh);
}