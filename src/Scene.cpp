#include <Cala/Scene.h>
#include <algorithm>
#include <Ende/thread/thread.h>
#include <Cala/Material.h>
#include <Cala/Probe.h>
#include <Ende/profile/profile.h>

cala::Scene::Scene(cala::Engine* engine, u32 count, u32 lightCount)
    : _engine(engine),
    _meshDataBuffer{engine->createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE, backend::MemoryProperties::HOST_CACHED | backend::MemoryProperties::HOST_VISIBLE),
                 engine->createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE, backend::MemoryProperties::HOST_CACHED | backend::MemoryProperties::HOST_VISIBLE)},
    _modelBuffer{engine->createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE, backend::MemoryProperties::HOST_CACHED | backend::MemoryProperties::HOST_VISIBLE),
                 engine->createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE, backend::MemoryProperties::HOST_CACHED | backend::MemoryProperties::HOST_VISIBLE)},
    _lightBuffer{engine->createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE),
                 engine->createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE)},
    _lightCountBuffer{engine->createBuffer(sizeof(u32) * 2 + sizeof(f32), backend::BufferUsage::STORAGE),
                 engine->createBuffer(sizeof(u32) * 2 + sizeof(f32), backend::BufferUsage::STORAGE)},
    _directionalLightCount(0),
    _lightsDirtyFrame(2)
{
    _mappedMesh[0] = _meshDataBuffer[0]->map();
    _mappedMesh[1] = _meshDataBuffer[1]->map();
    _mappedModel[0] = _modelBuffer[0]->map();
    _mappedModel[1] = _modelBuffer[1]->map();
    _mappedLight[0] = _lightBuffer[0]->map();
    _mappedLight[1] = _lightBuffer[1]->map();
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
    _lights.push(std::move(light));
    _lightsDirtyFrame = 2;
    return _lights.size() - 1;
}

void cala::Scene::addSkyLightMap(ImageHandle skyLightMap, bool equirectangular, bool hdr) {
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
void assignMemory(void* start, u32 offset, T& data) {
    u8* address = static_cast<u8*>(start) + offset;
    *reinterpret_cast<T*>(address) = data;
}

void cala::Scene::prepare(u32 frame, cala::Camera& camera) {
    PROFILE_NAMED("Scene::prepare");
    camera.updateFrustum();

    u32 objectCount = _renderables.size();
    // resize buffers to fit and update persistent mappings
    if (objectCount * sizeof(MeshData) >= _meshDataBuffer[frame]->size()) {
        _meshDataBuffer[frame] = _engine->resizeBuffer(_meshDataBuffer[frame], objectCount * sizeof(MeshData) * 2);
        _mappedMesh[frame] = _meshDataBuffer[frame]->map();
    }
    if (objectCount * sizeof(ende::math::Mat4f) >= _modelBuffer[frame]->size()) {
        _modelBuffer[frame] = _engine->resizeBuffer(_modelBuffer[frame], objectCount * sizeof(ende::math::Mat4f) * 2);
        _mappedModel[frame] = _modelBuffer[frame]->map();
    }

    for (u32 i = 0; i < _renderables.size(); i++) {
        auto& f = _renderables[i].first;
        auto& renderable = _renderables[i].second.first;
        auto& transform = _renderables[i].second.second;
        if (!transform)
            continue;
        if (transform->isDirty()) {
            f = 2;
            transform->setDirty(false);
        }
        if (f > 0) {
            u32 meshOffset = i * sizeof(MeshData);
            MeshData mesh{ renderable.firstIndex, renderable.indexCount, static_cast<u32>(renderable.materialInstance->getOffset() / renderable.materialInstance->material()->_setSize), 0, renderable.aabb.min, renderable.aabb.max };
            assignMemory(_mappedMesh[frame].address, meshOffset, mesh);

            u32 transformOffset = i * sizeof(ende::math::Mat4f);
            auto model = transform->toMat();
            assignMemory(_mappedModel[frame].address, transformOffset, model);
            f--;
        }
    }

    for (auto& light : _lights) {
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
            auto& light = _lights[i];
            auto data = light.data();
            if (light.shadowing())
                data.shadowIndex = _engine->getShadowMap(shadowIndex++).index();
            else
                data.shadowIndex = -1;

            _lightData.push(data);
        }
        if (_lightData.size() * sizeof(Light::Data) >= _lightBuffer[frame]->size()) {
            _lightBuffer[frame] = _engine->resizeBuffer(_lightBuffer[frame], _lightData.size() * sizeof(Light::Data) * 2);
            _mappedLight[frame] = _lightBuffer[frame]->map();
        }
        u32 lightCount[3] = { _directionalLightCount, static_cast<u32>(_lights.size() - _directionalLightCount), *reinterpret_cast<u32*>(&shadowBias) };
        _lightCountBuffer[frame]->data({ lightCount, sizeof(u32) * 3 });
        std::memcpy(_mappedLight[frame].address, _lightData.data(), static_cast<u32>(_lightData.size() * sizeof(Light::Data)));

        _lightsDirtyFrame--;
    }

    _engine->updateMaterialdata();
}