#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Ende/Vector.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/Transform.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Mesh.h>
#include <Cala/Light.h>
#include <Cala/Engine.h>

namespace cala {

    class Scene {
    public:

        struct Renderable {
            backend::vulkan::Buffer::View vertex;
            backend::vulkan::Buffer::View index;
            MaterialInstance* materialInstance = nullptr;
            ende::Span<VkVertexInputBindingDescription> bindings = nullptr;
            ende::Span<backend::Attribute> attributes = nullptr;
            bool castShadows = true;
        };

        Scene(Engine* engine, u32 count, u32 lightCount = 10);

        void addRenderable(Renderable&& renderable, Transform* transform);

        void addRenderable(Mesh& mesh, MaterialInstance* materialInstance, Transform* transform, bool castShadow = true);

        u32 addLight(Light& light);

        void addSkyLightMap(ImageHandle skyLightMap, bool equirectangular = false);

        void prepare(u32 frame, Camera& camera);

        void render(backend::vulkan::CommandBuffer& cmd);

//    private:

        Engine* _engine;

        ende::Vector<std::pair<Renderable, Transform*>> _renderables;
        ende::Vector<Light> _lights;
        u32 _directionalLightCount;

        BufferHandle _modelBuffer[2];
        BufferHandle _lightBuffer[2];
        BufferHandle _lightCountBuffer[2];
        ImageHandle _skyLightMap;
        backend::vulkan::Image::View _skyLightMapView;
        u32 _skyLight;

        ende::Vector<ende::math::Mat4f> _modelTransforms;
        ende::Vector<Light::Data> _lightData;
        ende::Vector<std::pair<u32, std::pair<Renderable, Transform*>>> _renderList;

        f32 shadowBias = 0.01;

    };

}

#endif //CALA_SCENE_H
