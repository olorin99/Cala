#include "Cala/Mesh.h"

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



cala::Mesh &cala::Mesh::addVertex(const Vertex &vertex) {
    _vertices.push(vertex);
    return *this;
}

cala::Mesh &cala::Mesh::addIndex(u32 index) {
    _indices.push(index);
    return *this;
}

cala::Mesh &cala::Mesh::addTriangle(u32 a, u32 b, u32 c) {
    _indices.push(a);
    _indices.push(b);
    _indices.push(c);
    return *this;
}

cala::Mesh &cala::Mesh::addQuad(u32 a, u32 b, u32 c, u32 d) {
    addTriangle(a, b, d);
    addTriangle(d, c, a);
    return *this;
}

cala::backend::vulkan::Buffer cala::Mesh::vertexBuffer(backend::vulkan::Driver& driver) const {
    backend::vulkan::Buffer buf(driver, _vertices.size() * sizeof(Vertex), backend::BufferUsage::VERTEX, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);
    buf.data({_vertices.data(), static_cast<u32>(_vertices.size() * sizeof(Vertex))});
    return std::move(buf);
}

cala::backend::vulkan::Buffer cala::Mesh::indexBuffer(backend::vulkan::Driver& driver) const {
    backend::vulkan::Buffer buf(driver, _indices.size() * sizeof(u32), backend::BufferUsage::INDEX, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);
    buf.data({_indices.data(), static_cast<u32>(_indices.size() * sizeof(u32))});
    return std::move(buf);
}