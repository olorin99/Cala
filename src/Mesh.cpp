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

cala::backend::vulkan::Buffer cala::Mesh::vertexBuffer(backend::vulkan::Context& context) const {
    backend::vulkan::Buffer buf(context, _vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    buf.data({_vertices.data(), static_cast<u32>(_vertices.size() * sizeof(Vertex))});
    return std::move(buf);
}

cala::backend::vulkan::Buffer cala::Mesh::indexBuffer(backend::vulkan::Context& context) const {
    backend::vulkan::Buffer buf(context, _indices.size() * sizeof(u32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    buf.data({_indices.data(), static_cast<u32>(_indices.size() * sizeof(u32))});
    return std::move(buf);
}