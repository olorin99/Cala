#ifndef CALA_MESHDATA_H
#define CALA_MESHDATA_H

#include <Ende/platform.h>
#include <Ende/Vector.h>
#include <array>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/Mesh.h>
#include <Cala/Engine.h>

namespace cala {

    struct Vertex {
        std::array<f32, 3> position = {0, 0, 0};
        std::array<f32, 3> normal = {0, 1, 0};
        std::array<f32, 2> texCoords = {0, 0};
        std::array<f32, 4> tangent = {1, 0, 0};
    };

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

        backend::vulkan::Buffer vertexBuffer(backend::vulkan::Driver& driver) const;

        backend::vulkan::Buffer indexBuffer(backend::vulkan::Driver& driver) const;

        Mesh mesh(Engine* engine);

    private:

        ende::Vector<Vertex> _vertices;
        ende::Vector<u32> _indices;

    };

}

#endif //CALA_MESHDATA_H
