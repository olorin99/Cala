#ifndef CALA_MESH_H
#define CALA_MESH_H

#include <Cala/backend/vulkan/Buffer.h>
#include <Ende/Vector.h>
#include <Cala/Engine.h>
#include <optional>

namespace cala {

    class Mesh {
    public:

        Mesh(BufferHandle vertex, BufferHandle index, VkVertexInputBindingDescription binding, std::array<backend::Attribute, 4> attributes);

    //private:

        BufferHandle _vertex;
        BufferHandle _index; // optional

        struct Primitive {
            u32 firstIndex;
            u32 indexCount;
        };
        ende::Vector<Primitive> _primitives;

        VkVertexInputBindingDescription _binding;
        std::array<backend::Attribute, 4> _attributes;

    };

//    struct Mesh {
//        BufferHandle vertexBuffer;
//        BufferHandle indexBuffer;
//        u32 firstIndex = 0;
//        u32 indexCount = 0;
//    };


}

#endif //CALA_MESH_H
