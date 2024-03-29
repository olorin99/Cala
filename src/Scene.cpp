#include <Cala/Scene.h>
#include <algorithm>
#include <Ende/thread/thread.h>
#include <Cala/Material.h>
#include <Ende/profile/profile.h>

cala::Scene::Scene(cala::Engine* engine, u32 count, u32 lightCount)
    : _engine(engine),
    _directionalLightCount(0),
    _lightsDirtyFrame(2)
{
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _meshDataBuffer[i] = engine->device().createBuffer({
            .size = (u32)(count * sizeof(GPUMesh)),
            .usage = vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::STAGING,
            .persistentlyMapped = true,
            .name = "MeshDataBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _meshTransformsBuffer[i] = engine->device().createBuffer({
            .size = (u32)(count * sizeof(ende::math::Mat4f)),
            .usage = vk::BufferUsage::UNIFORM | vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::STAGING,
            .persistentlyMapped = true,
            .name = "ModelBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _lightBuffer[i] = engine->device().createBuffer({
            .size = (u32)(sizeof(u32) + lightCount * sizeof(GPULight)),
            .usage = vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::STAGING,
            .persistentlyMapped = true,
            .name = "LightBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _cameraBuffer[i] = engine->device().createBuffer({
            .size = (u32)(10 * sizeof(GPUCamera)),
            .usage = vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::STAGING,
            .persistentlyMapped = true,
            .name = "CameraBuffer: " + std::to_string(i)
        });
    }

    _root = std::make_unique<SceneNode>();
}

void cala::Scene::addSkyLightMap(vk::ImageHandle skyLightMap, bool equirectangular, bool hdr) {
    if (equirectangular) {
        _skyLightMap = _engine->convertToCubeMap(skyLightMap);
    } else
        _skyLightMap = skyLightMap;

    _skyLightMapView = _skyLightMap->newView(0, 10);
    _skyLightIrradiance = _engine->generateIrradianceMap(_skyLightMap);
    _skyLightPrefilter = _engine->generatePrefilteredIrradiance(_skyLightMap);
    _hdrSkyLight = hdr;
}

void traverseNode(cala::Scene::SceneNode* node, ende::math::Mat4f worldTransform, std::vector<ende::math::Mat4f>& meshTransforms) {
    if (node->transform.isDirty()) {
        node->worldTransform = worldTransform * node->transform.local();
    }

    if (auto meshNode = dynamic_cast<cala::Scene::MeshNode*>(node); meshNode && node->transform.isDirty())
        meshTransforms[meshNode->index] = node->worldTransform;

    for (auto& child : node->children) {
        if (node->transform.isDirty())
            child->transform.setDirty(node->transform.isDirty());
        traverseNode(child.get(), node->worldTransform, meshTransforms);
    }
    node->transform.setDirty(false);
}

void cala::Scene::prepare() {
    PROFILE_NAMED("Scene::prepare");
    u32 frame = _engine->device().frameIndex();

    u32 meshCount = _meshData.size();
    // resize buffers to fit and update persistent mappings
    if (meshCount * sizeof(GPUMesh) >= _meshDataBuffer[frame]->size()) {
        _meshDataBuffer[frame] = _engine->device().resizeBuffer(_meshDataBuffer[frame], meshCount * sizeof(GPUMesh) * 2);
    }
    if (meshCount * sizeof(ende::math::Mat4f) >= _meshTransformsBuffer[frame]->size()) {
        _meshTransformsBuffer[frame] = _engine->device().resizeBuffer(_meshTransformsBuffer[frame], meshCount * sizeof(ende::math::Mat4f) * 2);
    }
    if (_lights.size() * sizeof(GPULight) >= _lightBuffer[frame]->size()) {
        _lightBuffer[frame] = _engine->device().resizeBuffer(_lightBuffer[frame], _lights.size() * sizeof(GPULight) * 2 + sizeof(u32));
    }

    // update transforms
    traverseNode(_root.get(), ende::math::identity<4, f32>(), _meshTransforms);

    _meshDataBuffer[frame]->data(_meshData);
//    _engine->stageData(_meshDataBuffer[frame], _meshData);
    _meshTransformsBuffer[frame]->data(_meshTransforms);
//    _engine->stageData(_meshTransformsBuffer[frame], _meshTransforms);

    _cameraData.clear();
    auto mainCamera = getMainCamera();
    mainCamera->updateFrustum();
    if (_updateCullingCamera) {
        _cullingCameraData = mainCamera->data();
    }
    _cameraData.push_back(_cullingCameraData);
    for (u32 cameraIndex = 0; cameraIndex < _cameras.size(); cameraIndex++) {
        _cameras[cameraIndex].updateFrustum();
        auto cameraData = _cameras[cameraIndex].data();
        _cameraData.push_back(cameraData);
    }

    _lightData.clear();
    for (u32 lightIndex = 0; lightIndex < _lights.size(); lightIndex++) {
        auto& light = _lights[lightIndex];
        auto data = light.data();

        if (light.type() == Light::DIRECTIONAL) {

            u32 cameraIndex = _cameraData.size();
            data.cameraIndex = cameraIndex;
            light.setCameraIndex(cameraIndex);

            for (u32 cascadeIndex = 0; cascadeIndex < light.getCascadeCount(); cascadeIndex++) {
                f32 near = mainCamera->near();
                f32 far = mainCamera->far();
                if (cascadeIndex > 0)
                    near = light.getCascadeSplit(cascadeIndex - 1);
                if (cascadeIndex < light.getCascadeCount() - 1)
                    far = light.getCascadeSplit(cascadeIndex);

                Camera cascadeCamera(mainCamera->fov(), mainCamera->width(), mainCamera->height(), near, far, &mainCamera->transform());
                auto frustumCorners = cascadeCamera.getFrustumCorners();

                ende::math::Vec3f center = { 0, 0, 0 };
                for (auto& corner : frustumCorners)
                    center = center + corner.xyz();
                center = center / frustumCorners.size();

                Transform cascadeTransform(center, mainCamera->transform().rot());
                cascadeCamera.setTransform(&cascadeTransform);

                ende::math::Vec3f min = { std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max() };
                ende::math::Vec3f max = { std::numeric_limits<f32>::lowest(), std::numeric_limits<f32>::lowest(), std::numeric_limits<f32>::lowest() };

                auto cascadeView = cascadeCamera.view();
                for (auto& corner : frustumCorners) {
                    auto cornerLightSpace = cascadeView.transform(corner);
                    min = {
                            std::min(min.x(), cornerLightSpace.x()),
                            std::min(min.y(), cornerLightSpace.y()),
                            std::min(min.z(), cornerLightSpace.z()),
                    };
                    max = {
                            std::max(max.x(), cornerLightSpace.x()),
                            std::max(max.y(), cornerLightSpace.y()),
                            std::max(max.z(), cornerLightSpace.z()),
                    };
                }

                const f32 mult = 10;
                if (min.z() < 0)
                    min[2] = min.z() * mult;
                else
                    min[2] = min.z() / mult;
                if (max.z() < 0)
                    max[2] = max.z() / mult;
                else
                    max[2] = max.z() * mult;

                cascadeTransform.setRot(light.getDirection());
                auto projection = ende::math::orthographic<f32>(min.x(), max.x(), min.y(), max.y(), min.z(), max.z());
                cascadeCamera.setProjection(projection);
                cascadeCamera.updateFrustum();

                _cameraData.push_back(cascadeCamera.data());

            }
        }

        _lightData.push_back(data);
    }
    _lightBuffer[frame]->data(_lightData, sizeof(u32));
//    _engine->stageData(_lightBuffer[frame], _lightData, sizeof(u32));
    u32 totalLightCount = _lights.size();
    _lightBuffer[frame]->data(totalLightCount);
//    _engine->stageData(_lightBuffer[frame], totalLightCount);

    if (_cameraData.size() * sizeof(GPUCamera) >= _cameraBuffer[frame]->size()) {
        _cameraBuffer[frame] = _engine->device().resizeBuffer(_cameraBuffer[frame], _cameraData.size() * sizeof(GPUCamera) * 2);
    }
    _cameraBuffer[frame]->data(_cameraData);
//    _engine->stageData(_cameraBuffer[frame], _cameraData);


    _engine->updateMaterialdata();
}

cala::Scene::SceneNode* cala::Scene::addNode(const std::string& name, const cala::Transform &transform, cala::Scene::SceneNode *parent) {
    auto node = std::make_unique<SceneNode>();
    node->type = NodeType::NONE;
    node->transform = transform;
    node->name = name;
    if (parent) {
        node->parent = parent;
        parent->children.push_back(std::move(node));
        return parent->children.back().get();
    } else {
        node->parent = _root.get();
        _root->children.push_back(std::move(node));
        return _root->children.back().get();
    }
}

cala::Scene::SceneNode* addModelNode(const std::string& name, cala::Scene& scene, cala::Model& model, cala::Model::Node& node, const cala::Transform& transform, cala::Scene::SceneNode* parent) {
    auto sceneNode = scene.addNode(node.name.empty() ? name : node.name, transform, parent);
    for (auto primitiveIndex : node.primitives) {
        auto& primitive = model.primitives[primitiveIndex];
        cala::Mesh mesh = {
            primitive.firstIndex,
            primitive.indexCount,
            primitive.meshletIndex,
            primitive.meshletCount,
            &model.materials[primitive.materialIndex],
            { primitive.aabb.min.x(), primitive.aabb.min.y(), primitive.aabb.min.z(), 1.0 },
            { primitive.aabb.max.x(), primitive.aabb.max.y(), primitive.aabb.max.z(), 1.0 }
        };
        for (u32 level = 0; level < MAX_LODS; level++) {
            mesh.lods[level].meshletOffset = primitive.lods[level].meshletOffset;
            mesh.lods[level].meshletCount = primitive.lods[level].meshletCount;
            mesh.lodCount = primitive.lodCount;
        }
        scene.addMesh(mesh, cala::Transform(), nullptr, sceneNode);
    }

    for (auto& child : node.children) {
        auto& modelNode = model.nodes[child];
        addModelNode(name, scene, model, modelNode, cala::Transform(), sceneNode);
    }
    return sceneNode;
}

cala::Scene::SceneNode *cala::Scene::addModel(const std::string& name, cala::Model &model, const cala::Transform& transform, cala::Scene::SceneNode *parent) {
    if (model.nodes.size() > 1) {
        auto rootNode = addNode(name, transform, parent);
        for (auto& node : model.nodes)
            addModelNode(name, *this, model, node, Transform(), rootNode);
        return rootNode;
    }
    return addModelNode(name, *this, model, model.nodes.front(), transform, parent);
}

cala::Scene::SceneNode *cala::Scene::addMesh(const cala::Mesh &mesh, const cala::Transform &transform, cala::MaterialInstance *materialInstance, cala::Scene::SceneNode *parent) {
    MaterialInstance* instance = materialInstance ? materialInstance : mesh.materialInstance;
    i32 index = _meshData.size();
    _meshData.push_back(GPUMesh{
        instance->material()->id(),
        instance->getIndex(),
        mesh.min,
        mesh.max,
        true,
        true,
        mesh.lodCount
    });
    for (u32 level = 0; level < MAX_LODS; level++) {
        _meshData.back().lods[level].meshletOffset = mesh.lods[level].meshletOffset;
        _meshData.back().lods[level].meshletCount = mesh.lods[level].meshletCount;
    }
    _meshes.push_back(mesh);
    // materialInstance can be changed by the parameter passed to function
    _meshes.back().materialInstance = instance;
    _meshTransforms.push_back(transform.local());
    assert(_meshData.size() == _meshTransforms.size());
    assert(_meshData.size() == _meshes.size());

    _min = {
            std::min(_min.x(), mesh.min.x()),
            std::min(_min.y(), mesh.min.y()),
            std::min(_min.z(), mesh.min.z())
    };
    _max = {
            std::max(_max.x(), mesh.max.x()),
            std::max(_max.y(), mesh.max.y()),
            std::max(_max.z(), mesh.max.z())
    };

    _totalMeshlets += _meshData.back().lods[0].meshletCount;

    auto node = std::make_unique<MeshNode>();
    node->index = index;
    node->type = NodeType::MESH;
    node->transform = transform;
    if (parent) {
        node->parent = parent;

        parent->children.push_back(std::move(node));
        return parent->children.back().get();
    } else { // add to root
        node->parent = _root.get();
        _root->children.push_back(std::move(node));
        return _root->children.back().get();
    }
}

cala::Scene::SceneNode *cala::Scene::addLight(const cala::Light &light, const cala::Transform &transform, cala::Scene::SceneNode *parent) {
    i32 index = _lights.size();
    _lights.push_back(light);
    auto node = std::make_unique<LightNode>();
    node->index = index;
    node->type = NodeType::LIGHT;
    node->transform = transform;
    if (parent) {
        node->parent = parent;
        parent->children.push_back(std::move(node));
        return parent->children.back().get();
    } else { // add to root
        node->parent = _root.get();
        _root->children.push_back(std::move(node));
        return _root->children.back().get();
    }
}

cala::Scene::SceneNode *cala::Scene::addCamera(const cala::Camera &camera, const cala::Transform &transform, cala::Scene::SceneNode *parent) {
    i32 index = _cameras.size();
    _cameras.push_back(camera);
    if (_mainCameraIndex < 0)
        _mainCameraIndex = index;
    auto node = std::make_unique<CameraNode>();
    node->index = index;
    node->type = NodeType::CAMERA;
    node->transform = transform;
    _cameras.back().setTransform(&node->transform);
    if (parent) {
        node->parent = parent;
        parent->children.push_back(std::move(node));
        return parent->children.back().get();
    } else {
        node->parent = _root.get();
        _root->children.push_back(std::move(node));
        return _root->children.back().get();
    }
}

void traverseNodeUpdateMeshIndices(cala::Scene::SceneNode* node, u32 meshIndex) {
    if (node->type == cala::Scene::NodeType::MESH) {
        auto meshNode = dynamic_cast<cala::Scene::MeshNode*>(node);
        if (meshNode->index > meshIndex) {
            meshNode->index--;
        }
    }
    for (auto& child : node->children)
        traverseNodeUpdateMeshIndices(child.get(), meshIndex);
}

void cala::Scene::removeChildNode(cala::Scene::SceneNode *parent, u32 childIndex) {
    if (!parent || parent->children.size() <= childIndex)
        return;

    auto child = parent->children[childIndex].get();
    if (child->type == NodeType::MESH) {
        auto meshNode = dynamic_cast<MeshNode*>(child);
        _meshData[meshNode->index].enabled = false;
        _meshes.erase(_meshes.begin() + meshNode->index);
        _meshData.erase(_meshData.begin() + meshNode->index);
        _meshTransforms.erase(_meshTransforms.begin() + meshNode->index);
        traverseNodeUpdateMeshIndices(_root.get(), meshNode->index);
    }

    while (!child->children.empty())
        removeChildNode(child, 0);
    parent->children.erase(parent->children.begin() + childIndex);
}

cala::Camera* cala::Scene::getCamera(cala::Scene::SceneNode *node) {
    assert(node->type == NodeType::CAMERA);
    return &_cameras[dynamic_cast<CameraNode*>(node)->index];
}

cala::Camera *cala::Scene::getMainCamera() {
    assert(_mainCameraIndex >= 0);
    return &_cameras[_mainCameraIndex];
}

void cala::Scene::setMainCamera(cala::Scene::CameraNode *node) {
    assert(node->type == NodeType::CAMERA);
    _mainCameraIndex = node->index;
    _cameras[_mainCameraIndex].setDirty(true);
}