#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Ende/Vector.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/Transform.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Mesh.h>

namespace cala {

    class Scene {
    public:

        struct Renderable {
            backend::vulkan::Buffer* vertex = nullptr;
            backend::vulkan::Buffer* index = nullptr;
            MaterialInstance* materialInstance = nullptr;
            ende::Span<VkVertexInputBindingDescription> bindings = nullptr;
            ende::Span<backend::Attribute> attributes = nullptr;
        };

        Scene(backend::vulkan::Driver& driver, u32 count);

        void addRenderable(Renderable&& renderable, Transform* transform);

        void addRenderable(Mesh& mesh, MaterialInstance* materialInstance, Transform* transform);

        void prepare();

        void render(backend::vulkan::CommandBuffer& cmd);

//    private:

        ende::Vector<std::pair<Renderable, Transform*>> _renderables;

        backend::vulkan::Buffer _modelBuffer;
        ende::Vector<ende::math::Mat4f> _modelTransforms;

    };

}

#endif //CALA_SCENE_H
