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
            .memoryType = vk::MemoryProperties::DEVICE,
            .name = "MeshDataBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _meshTransformsBuffer[i] = engine->device().createBuffer({
            .size = (u32)(count * sizeof(ende::math::Mat4f)),
            .usage = vk::BufferUsage::UNIFORM | vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::DEVICE,
            .name = "ModelBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _lightBuffer[i] = engine->device().createBuffer({
            .size = (u32)(lightCount * sizeof(GPULight)),
            .usage = vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::DEVICE,
            .name = "LightBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _lightCountBuffer[i] = engine->device().createBuffer({
            .size = (u32)(sizeof(u32) * 2),
            .usage = vk::BufferUsage::STORAGE,
            .memoryType = vk::MemoryProperties::DEVICE,
            .name = "LightCountBuffer: " + std::to_string(i)
        });
    }
    for (u32 i = 0; i < vk::FRAMES_IN_FLIGHT; i++) {
        _materialCountBuffer[i] = engine->device().createBuffer({
            .size = (u32)(sizeof(MaterialCount) * 1),
            .usage = vk::BufferUsage::UNIFORM | vk::BufferUsage::STORAGE | vk::BufferUsage::INDIRECT,
            .memoryType = vk::MemoryProperties::DEVICE,
            .name = "MaterialCountBuffer: " + std::to_string(i)
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

template <typename T>
void assignMemory(void* address, u32 offset, const T& data) {
    u8* start = static_cast<u8*>(address) + offset;
    *reinterpret_cast<T*>(start) = data;
}

template <typename T>
void assignMemory(void* address, u32 offset, T* data, u32 count) {
    u8* start = static_cast<u8*>(address) + offset;
    std::memcpy(start, data, count * sizeof(T));
}

void traverseNode(cala::Scene::SceneNode* node, ende::math::Mat4f worldTransform, std::vector<ende::math::Mat4f>& meshTransforms) {
    auto transform = worldTransform * node->transform.local();
    if (auto meshNode = dynamic_cast<cala::Scene::MeshNode*>(node); meshNode && node->transform.isDirty())
        meshTransforms[meshNode->index] = transform;

    for (auto& child : node->children) {
        child->transform.setDirty(node->transform.isDirty());
        traverseNode(child.get(), transform, meshTransforms);
    }
}

void cala::Scene::prepare(cala::Camera& camera) {
    PROFILE_NAMED("Scene::prepare");
    u32 frame = _engine->device().frameIndex();
    camera.updateFrustum();
    _materialCounts.resize(_engine->materialCount());
    for (auto& materialCount : _materialCounts)
        materialCount.count = 0;


    u32 meshCount = _meshData.size();
    // resize buffers to fit and update persistent mappings
    if (meshCount * sizeof(GPUMesh) >= _meshDataBuffer[frame]->size()) {
        _meshDataBuffer[frame] = _engine->device().resizeBuffer(_meshDataBuffer[frame], meshCount * sizeof(GPUMesh) * 2);
    }
    if (meshCount * sizeof(ende::math::Mat4f) >= _meshTransformsBuffer[frame]->size()) {
        _meshTransformsBuffer[frame] = _engine->device().resizeBuffer(_meshTransformsBuffer[frame], meshCount * sizeof(ende::math::Mat4f) * 2);
    }
    if (_materialCounts.size() * sizeof(MaterialCount) > _materialCountBuffer[frame]->size()) {
        _materialCountBuffer[frame] = _engine->device().resizeBuffer(_materialCountBuffer[frame], _materialCounts.size() * sizeof(MaterialCount));
    }
    if (_lightData.size() * sizeof(GPULight) >= _lightBuffer[frame]->size()) {
        _lightBuffer[frame] = _engine->device().resizeBuffer(_lightBuffer[frame], _lightData.size() * sizeof(GPULight) * 2);
    }

    // update transforms
    traverseNode(_root.get(), _root->transform.local(), _meshTransforms);

    _engine->stageData(_meshDataBuffer[frame], _meshData);
    _engine->stageData(_meshTransformsBuffer[frame], _meshTransforms);

    std::sort(_lightData.begin(), _lightData.end(), [](const GPULight& lhs, const GPULight& rhs) {
        return lhs.type < rhs.type;
    });
    _lightData.clear();
    for (u32 lightIndex = 0; lightIndex < _lights.size(); lightIndex++) {
        auto& light = _lights[lightIndex].second;
        auto data = light.data();
        data.cameraIndex = lightIndex + 1;
        _lightData.push_back(data);
    }
    _engine->stageData(_lightBuffer[frame], _lightData);

    u32 lightCount[2] = { _directionalLightCount, static_cast<u32>(_lights.size() - _directionalLightCount) };
    std::span<const u8> ls(reinterpret_cast<const u8*>(lightCount), sizeof(u32) * 2);
    _engine->stageData(_lightCountBuffer[frame], ls);


    u32 offset = 0;
    for (auto& count : _materialCounts) {
        count.offset = offset;
        offset += count.count;
    }
    _engine->stageData(_materialCountBuffer[frame], _materialCounts);

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

cala::Scene::SceneNode* addModelNode(cala::Scene& scene, cala::Model& model, cala::Model::Node& node, const cala::Transform& transform, cala::Scene::SceneNode* parent) {
    auto sceneNode = scene.addNode(node.name, transform, parent);
    for (auto primitiveIndex : node.primitives) {
        auto& primitive = model.primitives[primitiveIndex];
        scene.addMesh({
            primitive.firstIndex,
            primitive.indexCount,
            &model.materials[primitive.materialIndex],
            { primitive.aabb.min.x(), primitive.aabb.min.y(), primitive.aabb.min.z(), 1.0 },
            { primitive.aabb.max.x(), primitive.aabb.max.y(), primitive.aabb.max.z(), 1.0 },
        }, cala::Transform(), nullptr, sceneNode);
    }

    for (auto& child : node.children) {
        auto& modelNode = model.nodes[child];
        addModelNode(scene, model, modelNode, cala::Transform(), sceneNode);
    }
    return sceneNode;
}

cala::Scene::SceneNode *cala::Scene::addModel(cala::Model &model, const cala::Transform& transform, cala::Scene::SceneNode *parent) {
    auto& rootNode = model.nodes.front();
    return addModelNode(*this, model, rootNode, transform, parent);
}

cala::Scene::SceneNode *cala::Scene::addMesh(const cala::Mesh &mesh, const cala::Transform &transform, cala::MaterialInstance *materialInstance, cala::Scene::SceneNode *parent) {
    MaterialInstance* instance = materialInstance ? materialInstance : mesh.materialInstance;
    i32 index = _meshData.size();
    _meshData.push_back(GPUMesh{
        mesh.firstIndex,
        mesh.indexCount,
        instance->material()->id(),
        instance->getIndex(),
        mesh.min,
        mesh.max
    });
    _meshes.push_back({
        mesh.firstIndex,
        mesh.indexCount,
        instance,
        mesh.min,
        mesh.max
    });
    _meshTransforms.push_back(transform.local());
    assert(_meshData.size() == _meshTransforms.size());
    assert(_meshData.size() == _meshes.size());
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
    _lights.push_back(std::make_pair(2, light));
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