#include <Cala/Scene.h>
#include <algorithm>
#include <Ende/thread/thread.h>
#include <Cala/Material.h>
#include <Cala/Probe.h>
#include <Ende/profile/profile.h>

cala::Scene::Scene(cala::Engine* engine, u32 count, u32 lightCount)
    : _engine(engine),
    _meshDataBuffer{engine->createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE),
                 engine->createBuffer(count * sizeof(MeshData), backend::BufferUsage::STORAGE)},
    _modelBuffer{engine->createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE),
                 engine->createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM | backend::BufferUsage::STORAGE)},
    _lightBuffer{engine->createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE),
                 engine->createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE)},
    _lightCountBuffer{engine->createBuffer(sizeof(u32) * 2 + sizeof(f32), backend::BufferUsage::STORAGE),
                 engine->createBuffer(sizeof(u32) * 2 + sizeof(f32), backend::BufferUsage::STORAGE)},
    _directionalLightCount(0),
    _objectsDirtyFrames(2),
    _lightsDirtyFrame(2)
{}


void cala::Scene::addRenderable(cala::Scene::Renderable &&renderable, cala::Transform *transform) {
    u32 key = 0;

    _renderables.push(std::make_pair(renderable, transform));
    _objectsDirtyFrames = 2;
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
    _hdrSkyLight = hdr;
}

void cala::Scene::prepare(u32 frame, cala::Camera& camera) {
    PROFILE_NAMED("Scene::prepare");
    camera.updateFrustum();

    if (_objectsDirtyFrames > 0) {
        _renderList.clear();
        for (auto& item : _renderables) {
            ende::math::Vec4f pos{ item.second->pos().x(), item.second->pos().y(), item.second->pos().z(), 1 };
            auto a = camera.viewProjection().transform(pos);
            a = a / a.w();
            f32 depth = a.z();
            u32 index = *reinterpret_cast<u32*>(&depth);
//        u32 index = depth * 10000000;
            _renderList.push(std::make_pair(index, item));
        }

//        std::sort(_renderList.begin(), _renderList.end(), [](const std::pair<u32, std::pair<Renderable, Transform*>>& lhs, const std::pair<u32, std::pair<Renderable, Transform*>>& rhs) {
//            return lhs.first < rhs.first;
//        });

        _meshData.clear();
        _modelTransforms.clear();
        for (auto& renderablePair : _renderList) {
            auto& renderable = renderablePair.second.first;
            auto& transform = renderablePair.second.second;
            _meshData.push({ renderable.firstIndex, renderable.indexCount, static_cast<u32>(renderable.materialInstance->getOffset() / renderable.materialInstance->material()->_setSize), 0, renderable.aabb.min, renderable.aabb.max });
            _modelTransforms.push(transform->toMat());
        }
        if (_meshData.size() * sizeof(MeshData) >= _meshDataBuffer[frame]->size())
            _meshDataBuffer[frame] = _engine->resizeBuffer(_meshDataBuffer[frame], _meshData.size() * sizeof(MeshData) * 2);
        _meshDataBuffer[frame]->data({_meshData.data(), static_cast<u32>(_meshData.size() * sizeof(MeshData))});

        if (_modelTransforms.size() * sizeof(ende::math::Mat4f) >= _modelBuffer[frame]->size())
            _modelBuffer[frame] = _engine->resizeBuffer(_modelBuffer[frame], _modelTransforms.size() * sizeof(ende::math::Mat4f) * 2);
        _modelBuffer[frame]->data({_modelTransforms.data(), static_cast<u32>(_modelTransforms.size() * sizeof(ende::math::Mat4f))});

        _objectsDirtyFrames--;
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
                data.shadowIndex = _engine->_defaultPointShadow.index();

            _lightData.push(data);
        }
        if (_lightData.size() * sizeof(Light::Data) >= _lightBuffer[frame]->size())
            _lightBuffer[frame] = _engine->resizeBuffer(_lightBuffer[frame], _lightData.size() * sizeof(Light::Data) * 2);
        u32 lightCount[3] = { _directionalLightCount, static_cast<u32>(_lights.size() - _directionalLightCount), *reinterpret_cast<u32*>(&shadowBias) };
        _lightCountBuffer[frame]->data({ lightCount, sizeof(u32) * 3 });
        _lightBuffer[frame]->data({_lightData.data(), static_cast<u32>(_lightData.size() * sizeof(Light::Data))});

        _lightsDirtyFrame--;
    }

    _engine->updateMaterialdata();
}

void cala::Scene::render(backend::vulkan::CommandBuffer& cmd) {

    Material* material = nullptr;
    MaterialInstance* materialInstance = nullptr;

    u32 lightCount = _lightData.empty() ? 1 : _lightData.size();

    for (u32 light = 0; light < lightCount; light++) {
        if (!_lightData.empty())
            cmd.bindBuffer(3, 0, *_lightBuffer[0], light * sizeof(Light::Data), sizeof(Light::Data));

        for (u32 i = 0; i < _renderList.size(); i++) {
            auto& renderable = _renderList[i].second.first;
            auto& transform = _renderList[i].second.second;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            if (materialInstance != renderable.materialInstance) {
                materialInstance = renderable.materialInstance;
                if (materialInstance) {
                    if (materialInstance->material() != material) {
                        material = materialInstance->material();
                    }
                    materialInstance->bind(cmd);
                }
            }
            if (material) {
                cmd.bindProgram(*material->getProgram());
                cmd.bindRasterState(material->_rasterState);
                cmd.bindDepthState(material->_depthState);
            }

            cmd.bindBuffer(1, 0, *_modelBuffer[0], i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

            cmd.bindPipeline();
            cmd.bindDescriptors();

            cmd.bindVertexBuffer(0, renderable.vertex->buffer());
            if (renderable.index)
                cmd.bindIndexBuffer(*renderable.index);

            if (renderable.index)
                cmd.draw(renderable.index->size() / sizeof(u32), 1, 0, 0);
            else
                cmd.draw(renderable.vertex->size() / (4 * 14), 1, 0, 0);
        }
    }
}