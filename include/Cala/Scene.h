#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Ende/Vector.h>
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
            ende::Span<VkVertexInputBindingDescription> bindings = nullptr;
            ende::Span<backend::Attribute> attributes = nullptr;
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

        ende::Vector<std::pair<i32, std::pair<Renderable, Transform*>>> _renderables;
        ende::Vector<Light> _lights;
        u32 _directionalLightCount;

        i32 _lightsDirtyFrame;

        backend::vulkan::BufferHandle _meshDataBuffer[2];
        backend::vulkan::Buffer::Mapped _mappedMesh[2];
        backend::vulkan::BufferHandle _modelBuffer[2];
        backend::vulkan::Buffer::Mapped _mappedModel[2];
        backend::vulkan::BufferHandle _lightBuffer[2];
        backend::vulkan::Buffer::Mapped _mappedLight[2];
        backend::vulkan::BufferHandle _lightCountBuffer[2];
        backend::vulkan::BufferHandle _materialCountBuffer[2];
        backend::vulkan::Buffer::Mapped _mappedMaterialCounts[2];
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
        ende::Vector<MeshData> _meshData;
        ende::Vector<ende::math::Mat4f> _modelTransforms;
        ende::Vector<Light::Data> _lightData;

        struct MaterialCount {
            u32 count = 0;
            u32 offset = 0;
        };
        ende::Vector<MaterialCount> _materialCounts;

    };

}

#endif //CALA_SCENE_H
