#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/Transform.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Mesh.h>
#include <Cala/Model.h>
#include <Cala/Light.h>
#include <Cala/Engine.h>

namespace cala {

    class Scene {
    public:

        struct AABB {
            ende::math::Vec4f min;
            ende::math::Vec4f max;
        };

        struct Renderable {
            backend::vulkan::BufferHandle vertex;
            backend::vulkan::BufferHandle index;
            u32 firstIndex = 0;
            u32 indexCount = 0;
            MaterialInstance* materialInstance = nullptr;
            std::span<VkVertexInputBindingDescription> bindings = {};
            std::span<backend::Attribute> attributes = {};
            bool castShadows = true;
            AABB aabb = {};
        };

        Scene(Engine* engine, u32 count, u32 lightCount = 10);

        void addRenderable(Renderable&& renderable, Transform* transform);

        void addRenderable(Mesh& mesh, MaterialInstance* materialInstance, Transform* transform, bool castShadow = true);

        void addRenderable(Model& model, Transform* transform, bool castShadow = true);

        u32 addLight(Light& light);

        void addSkyLightMap(backend::vulkan::ImageHandle skyLightMap, bool equirectangular = false, bool hdr = true);

        void prepare(Camera& camera);

//    private:

        Engine* _engine;

        std::vector<std::pair<i32, std::pair<Renderable, Transform*>>> _renderables;
        std::vector<std::pair<i32, Light>> _lights;
        u32 _directionalLightCount;

        i32 _lightsDirtyFrame;

        backend::vulkan::BufferHandle _meshDataBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
        backend::vulkan::BufferHandle _modelBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
        backend::vulkan::BufferHandle _lightBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
        backend::vulkan::BufferHandle _lightCountBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
        backend::vulkan::BufferHandle _materialCountBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
        backend::vulkan::ImageHandle _skyLightMap;
        backend::vulkan::Image::View _skyLightMapView;
        backend::vulkan::ImageHandle _skyLightIrradiance;
        backend::vulkan::ImageHandle _skyLightPrefilter;
        bool _hdrSkyLight;
        u32 _skyLight;

        struct MeshData {
            u32 firstIndex;
            u32 indexCount;
            u32 materialID;
            u32 materialOffset;
            ende::math::Vec4f min;
            ende::math::Vec4f max;
        };
        std::vector<MeshData> _meshData;
        std::vector<ende::math::Mat4f> _modelTransforms;
        std::vector<Light::Data> _lightData;

        struct MaterialCount {
            u32 count = 0;
            u32 offset = 0;
        };
        std::vector<MaterialCount> _materialCounts;

    };

}

#endif //CALA_SCENE_H
