#ifndef CALA_MESHDATA_H
#define CALA_MESHDATA_H

#include <Ende/platform.h>
#include <array>
#include <Cala/vulkan/Buffer.h>
#include <Cala/Mesh.h>
#include <Cala/Engine.h>

namespace cala {

    bool operator==(const Vertex& lhs, const Vertex& rhs);

    bool operator!=(const Vertex& lhs, const Vertex& rhs);

//    template <typename T = Vertex>
    class MeshData {
    public:

        MeshData& addVertex(const Vertex& vertex);

        MeshData& addIndex(u32 index);

        MeshData& addTriangle(u32 a, u32 b, u32 c);

        MeshData& addQuad(u32 a, u32 b, u32 c, u32 d);


        u32 vertexCount() const { return _vertices.size(); }

        u32 indexCount() const { return _indices.size(); }

        vk::BufferHandle vertexBuffer(vk::Device& driver) const;

        vk::BufferHandle indexBuffer(vk::Device& driver) const;

        Mesh mesh(Engine* engine);

    private:

        std::vector<Vertex> _vertices;
        std::vector<u32> _indices;

    };

}

#endif //CALA_MESHDATA_H
