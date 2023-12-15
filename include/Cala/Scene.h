#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Cala/vulkan/Buffer.h>
#include <Cala/vulkan/CommandBuffer.h>
#include <Cala/Transform.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Mesh.h>
#include <Cala/Model.h>
#include <Cala/Light.h>
#include <Cala/Engine.h>
#include <Cala/shaderBridge.h>

namespace cala {

    class Scene {
    public:


        Scene(Engine* engine, u32 count, u32 lightCount = 10);

        void addSkyLightMap(vk::ImageHandle skyLightMap, bool equirectangular = false, bool hdr = true);

        void prepare();

        u32 meshCount() const { return _meshData.size(); }

        u32 lightCount() const { return _lights.size(); }

        enum class NodeType {
            NONE = 0,
            MESH = 1,
            LIGHT = 2,
            CAMERA = 3
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

        struct CameraNode : public SceneNode {
            i32 index;
        };

        SceneNode* addNode(const std::string& name, const Transform& transform, SceneNode* parent = nullptr);

        SceneNode* addModel(const std::string& name, Model& model, const Transform& transform, SceneNode* parent = nullptr);

        SceneNode* addMesh(const Mesh& mesh, const Transform& transform, MaterialInstance* materialInstance = nullptr, SceneNode* parent = nullptr);

        SceneNode* addLight(const Light& light, const Transform& transform, SceneNode* parent = nullptr);

        SceneNode* addCamera(const Camera& camera, const Transform& transform, SceneNode* parent = nullptr);

        void removeChildNode(SceneNode* parent, u32 childIndex);


        Camera* getCamera(SceneNode* node);

        Camera* getMainCamera();

        i32 getMainCameraIndex() const { return _mainCameraIndex + 1; }

        void setMainCamera(CameraNode* node);

//    private:

        Engine* _engine;

        u32 _directionalLightCount;

        i32 _lightsDirtyFrame;

        vk::BufferHandle _meshDataBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _meshTransformsBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _lightBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _cameraBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _materialCountBuffer[vk::FRAMES_IN_FLIGHT];
        vk::ImageHandle _skyLightMap;
        vk::Image::View _skyLightMapView;
        vk::ImageHandle _skyLightIrradiance;
        vk::ImageHandle _skyLightPrefilter;
        bool _hdrSkyLight;
        u32 _skyLight;

        std::vector<GPUMesh> _meshData;
        std::vector<ende::math::Mat4f> _meshTransforms;
        std::vector<GPULight> _lightData;
        std::vector<GPUCamera> _cameraData;

        std::vector<Mesh> _meshes;
        std::vector<std::pair<i32, Light>> _lights;
        std::vector<Camera> _cameras;
        i32 _mainCameraIndex = -1;
        GPUCamera _cullingCameraData = {};
        i32 _cullingCameraIndex = -1;
        bool _updateCullingCamera = true;

        struct MaterialCount {
            u32 count = 0;
            u32 offset = 0;
        };
        std::vector<MaterialCount> _materialCounts;

        std::unique_ptr<SceneNode> _root;

    };

}

#endif //CALA_SCENE_H
