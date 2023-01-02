#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Ende/Vector.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/Transform.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Mesh.h>
#include <Cala/Light.h>

namespace cala {

    class Scene {
    public:

        struct Renderable {
            backend::vulkan::Buffer::View vertex;
            backend::vulkan::Buffer::View index;
            MaterialInstance* materialInstance = nullptr;
            ende::Span<VkVertexInputBindingDescription> bindings = nullptr;
            ende::Span<backend::Attribute> attributes = nullptr;
        };

        Scene(backend::vulkan::Driver& driver, u32 count, u32 lightCount = 10);

        void addRenderable(Renderable&& renderable, Transform* transform);

        void addRenderable(Mesh& mesh, MaterialInstance* materialInstance, Transform* transform, bool castShadow = false);

        void addLight(Light& light);

        void prepare(Camera& camera);

        void render(backend::vulkan::CommandBuffer& cmd);

//    private:

        ende::Vector<std::pair<u32, std::pair<Renderable, Transform*>>> _renderables;
        ende::Vector<Light> _lights;

        backend::vulkan::Buffer _modelBuffer;
        ende::Vector<ende::math::Mat4f> _modelTransforms;
        backend::vulkan::Buffer _lightBuffer;
        ende::Vector<Light::Data> _lightData;
        ende::Vector<std::pair<u32, std::pair<Renderable, Transform*>>> _renderList;

    };

}

#endif //CALA_SCENE_H
