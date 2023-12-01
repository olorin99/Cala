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


        Scene(Engine* engine, u32 count, u32 lightCount = 10);

        void addSkyLightMap(backend::vulkan::ImageHandle skyLightMap, bool equirectangular = false, bool hdr = true);

        void prepare(Camera& camera);

        u32 meshCount() const { return _meshData.size(); }

        u32 lightCount() const { return _lights.size(); }

//    private:

        Engine* _engine;

        u32 _directionalLightCount;

        i32 _lightsDirtyFrame;

        backend::vulkan::BufferHandle _meshDataBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
        backend::vulkan::BufferHandle _meshTransformsBuffer[backend::vulkan::FRAMES_IN_FLIGHT];
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
        std::vector<ende::math::Mat4f> _meshTransforms;
        std::vector<Light::Data> _lightData;


        std::vector<Mesh> _meshes;
        std::vector<std::pair<i32, Light>> _lights;

        struct MaterialCount {
            u32 count = 0;
            u32 offset = 0;
        };
        std::vector<MaterialCount> _materialCounts;

        enum class NodeType {
            NONE = 0,
            MESH = 1,
            LIGHT = 2
        };

        struct SceneNode {
            virtual ~SceneNode() = default;

            NodeType type = NodeType::NONE;
            Transform transform = {};
            std::string name;
            SceneNode* parent = nullptr;
            std::vector<std::unique_ptr<SceneNode>> children;
        };

        struct MeshNode : public SceneNode {
            i32 index;
        };

        struct LightNode : public SceneNode {
            i32 index;
        };

        std::unique_ptr<SceneNode> _root;

        SceneNode* addNode(const std::string& name, const Transform& transform, SceneNode* parent = nullptr);

        SceneNode* addModel(Model& model, const Transform& transform, SceneNode* parent = nullptr);

        SceneNode* addMesh(const Mesh& mesh, const Transform& transform, MaterialInstance* materialInstance = nullptr, SceneNode* parent = nullptr);

        SceneNode* addLight(const Light& light, const Transform& transform, SceneNode* parent = nullptr);

    };

}

#endif //CALA_SCENE_H
