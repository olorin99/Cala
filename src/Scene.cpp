#include <Cala/Scene.h>
#include <algorithm>
#include <Ende/thread/thread.h>
#include <Cala/Material.h>
#include <Cala/Probe.h>
#include <Ende/profile/profile.h>

cala::Scene::Scene(cala::Engine* engine, u32 count, u32 lightCount)
    : _engine(engine),
//    _meshDataBuffer{engine->device().createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true),
//                 engine->device().createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true)},
//    _modelBuffer{engine->device().createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true),
//                 engine->device().createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true)},
//    _lightBuffer{engine->device().createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true),
//                 engine->device().createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true)},
//    _lightCountBuffer{engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true),
//                 engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true)},
//    _materialCountBuffer{engine->device().createBuffer(sizeof(MaterialCount) * 1, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::STAGING, true),
//                 engine->device().createBuffer(sizeof(MaterialCount) * 1, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::STAGING, true)},
    _directionalLightCount(0),
    _lightsDirtyFrame(2)
{
    for (u32 i = 0; i < backend::vulkan::FRAMES_IN_FLIGHT; i++) {
        _meshDataBuffer[i] = engine->device().createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true);
        std::string debugLabel = "MeshDataBuffer: " + std::to_string(i);
        engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_meshDataBuffer[i]->buffer(), debugLabel);
    }
    for (u32 i = 0; i < backend::vulkan::FRAMES_IN_FLIGHT; i++) {
        _modelBuffer[i] = engine->device().createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true);
        std::string debugLabel = "ModelBuffer: " + std::to_string(i);
        engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_modelBuffer[i]->buffer(), debugLabel);
    }
    for (u32 i = 0; i < backend::vulkan::FRAMES_IN_FLIGHT; i++) {
        _lightBuffer[i] = engine->device().createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true);
        std::string debugLabel = "LightBuffer: " + std::to_string(i);
        engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_lightBuffer[i]->buffer(), debugLabel);
    }
    for (u32 i = 0; i < backend::vulkan::FRAMES_IN_FLIGHT; i++) {
        _lightCountBuffer[i] = engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE, backend::MemoryProperties::STAGING, true);
        std::string debugLabel = "LightCountBuffer: " + std::to_string(i);
        engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_lightCountBuffer[i]->buffer(), debugLabel);
    }
    for (u32 i = 0; i < backend::vulkan::FRAMES_IN_FLIGHT; i++) {
        _materialCountBuffer[i] = engine->device().createBuffer(sizeof(MaterialCount) * 1, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::STAGING, true);
        std::string debugLabel = "MaterialCountBuffer: " + std::to_string(i);
        engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_materialCountBuffer[i]->buffer(), debugLabel);
    }
}


void cala::Scene::addRenderable(cala::Scene::Renderable &&renderable, cala::Transform *transform) {
    u32 key = 0;

    assert(transform);
    _renderables.push(std::make_pair(2, std::make_pair(renderable, transform)));
}

void cala::Scene::addRenderable(cala::Mesh &mesh, cala::MaterialInstance *materialInstance, cala::Transform *transform, bool castShadow) {
    addRenderable({
        mesh._vertex,
        mesh._index,
        mesh.firstIndex,
        mesh.indexCount,
        materialInstance,
        { &mesh._binding, 1 },
        mesh._attributes,
        castShadow,
        { { -1, -1, -1 }, { 1, 1, 1 } }
    }, transform);
}

void cala::Scene::addRenderable(Model &model, Transform *transform, bool castShadow) {
    for (auto& primitive : model.primitives) {
        auto& matInstance = model.materials[primitive.materialIndex];
        addRenderable({
            model.vertexBuffer,
            model.indexBuffer,
            primitive.firstIndex,
            primitive.indexCount,
            &matInstance,
            { &model._binding, 1 },
            model._attributes,
            castShadow,
            { { primitive.aabb.min.x(), primitive.aabb.min.y(), primitive.aabb.min.z(), 1.f }, { primitive.aabb.max.x(), primitive.aabb.max.y(), primitive.aabb.max.z(), 1.f } }
        }, transform);
    }
}

u32 cala::Scene::addLight(cala::Light &light) {
    if (light.type() == Light::DIRECTIONAL)
        ++_directionalLightCount;
    _lights.push(std::make_pair(2, std::move(light)));
    _lightsDirtyFrame = 2;
    return _lights.size() - 1;
}

void cala::Scene::addSkyLightMap(backend::vulkan::ImageHandle skyLightMap, bool equirectangular, bool hdr) {
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

void cala::Scene::prepare(cala::Camera& camera) {
    PROFILE_NAMED("Scene::prepare");
    u32 frame = _engine->device().frameIndex();
    camera.updateFrustum();
    _materialCounts.resize(_engine->materialCount());
    for (auto& materialCount : _materialCounts)
        materialCount.count = 0;

    u32 objectCount = _renderables.size();
    // resize buffers to fit and update persistent mappings
    if (objectCount * sizeof(MeshData) >= _meshDataBuffer[frame]->size()) {
        _meshDataBuffer[frame] = _engine->device().resizeBuffer(_meshDataBuffer[frame], objectCount * sizeof(MeshData) * 2);
        std::string debugLabel = "MeshDataBuffer: " + std::to_string(frame);
        _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_meshDataBuffer[frame]->buffer(), debugLabel);
    }
    if (objectCount * sizeof(ende::math::Mat4f) >= _modelBuffer[frame]->size()) {
        _modelBuffer[frame] = _engine->device().resizeBuffer(_modelBuffer[frame], objectCount * sizeof(ende::math::Mat4f) * 2);
        std::string debugLabel = "ModelBuffer: " + std::to_string(frame);
        _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_modelBuffer[frame]->buffer(), debugLabel);
    }
    if (_materialCounts.size() * sizeof(MaterialCount) > _materialCountBuffer[frame]->size()) {
        _materialCountBuffer[frame] = _engine->device().resizeBuffer(_materialCountBuffer[frame], _materialCounts.size() * sizeof(MaterialCount));
        std::string debugLabel = "MaterialCountBuffer: " + std::to_string(frame);
        _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_materialCountBuffer[frame]->buffer(), debugLabel);
    }

    for (u32 i = 0; i < _renderables.size(); i++) {
        auto& f = _renderables[i].first;
        auto& renderable = _renderables[i].second.first;
        auto& transform = _renderables[i].second.second;
        if (!transform)
            continue;

        auto material = renderable.materialInstance->material();
        _materialCounts[material->id()].count++;

        transform->updateWorld();
        if (transform->isDirty()) {
            f = 2;
            transform->setDirty(false);
        }
        if (f > 0) {
            u32 meshOffset = i * sizeof(MeshData);
            MeshData mesh{ renderable.firstIndex, renderable.indexCount, material->id(), static_cast<u32>(renderable.materialInstance->getOffset() / renderable.materialInstance->material()->size()), renderable.aabb.min, renderable.aabb.max };
            assignMemory(_meshDataBuffer[frame]->persistentMapping(), meshOffset, mesh);
            _meshDataBuffer[frame]->invalidate();

            u32 transformOffset = i * sizeof(ende::math::Mat4f);
            auto model = transform->world();
            assignMemory(_modelBuffer[frame]->persistentMapping(), transformOffset, model);
            _modelBuffer[frame]->invalidate();
            f--;
        }
    }

//    u32 lightCount[2] = { _directionalLightCount, static_cast<u32>(_lights.size() - _directionalLightCount) };
//    _lightCountBuffer[frame]->data({ lightCount, sizeof(u32) * 2 });
//    u32 shadowIndex = 0;
//    for(u32 i = 0; i < _lights.size(); i++) {
//        auto& f = _lights[i].first;
//        auto& light = _lights[i].second;
//
//        if (light.isDirty()) {
//            f = 2;
//            light.setDirty(false);
//        }
//        if (f > 0) {
//            u32 lightOffset = i * sizeof(Light::Data);
//            auto lightData = light.data();
//            if (light.shadowing())
//                lightData.shadowIndex = _engine->getShadowMap(shadowIndex++).index();
//            else
//                lightData.shadowIndex = -1;
//            assignMemory(_lightBuffer[frame]->persistentMapping(), lightOffset, lightData);
//            f--;
//        }
//    }

    for (auto& [f, light] : _lights) {
        if (light.isDirty()) {
            _lightsDirtyFrame = 2;
            light.setDirty(false);
        }
    }


    if (_lightsDirtyFrame > 0) {
        std::sort(_lightData.begin(), _lightData.end(), [](const Light::Data& lhs, const Light::Data& rhs) {
            return lhs.type < rhs.type;
        });
        _lightData.clear();
        u32 shadowIndex = 0;
        for (u32 i = 0; i < _lights.size(); i++) {
            auto& light = _lights[i].second;
            auto data = light.data();
            if (light.shadowing())
                data.shadowIndex = _engine->getShadowMap(shadowIndex++).index();
            else
                data.shadowIndex = -1;

            _lightData.push(data);
        }
        if (_lightData.size() * sizeof(Light::Data) >= _lightBuffer[frame]->size()) {
            _lightBuffer[frame] = _engine->device().resizeBuffer(_lightBuffer[frame], _lightData.size() * sizeof(Light::Data) * 2);
            std::string debugLabel = "LightBuffer: " + std::to_string(frame);
            _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_lightBuffer[frame]->buffer(), debugLabel);
        }
        u32 lightCount[2] = { _directionalLightCount, static_cast<u32>(_lights.size() - _directionalLightCount) };
        _lightCountBuffer[frame]->data({ lightCount, sizeof(u32) * 2 });
//        ende::log::info("lightData size: {}", _lightBuffer[frame]->size());
        assert(_lightData.size() * sizeof(Light::Data) <= _lightBuffer[frame]->size());
//        assignMemory(_lightBuffer[frame]->persistentMapping(), 0, _lights.size());
        assignMemory(_lightBuffer[frame]->persistentMapping(), 0, _lightData.data(), _lightData.size());
//        assignMemory(_lightBuffer[frame]->persistentMapping(), sizeof(u32) * 4, _lightData.data(), _lightData.size());
        //TODO: find reason for crash when copying memory at offset into mapped lightbuffer memory

        _lightBuffer[frame]->invalidate();
        _lightsDirtyFrame--;
    }

    u32 offset = 0;
    for (auto& count : _materialCounts) {
        count.offset = offset;
        offset += count.count;
    }
    std::memcpy(_materialCountBuffer[frame]->persistentMapping(), _materialCounts.data(), _materialCounts.size() * sizeof(MaterialCount));
    _materialCountBuffer[frame]->invalidate();

    _engine->updateMaterialdata();
}