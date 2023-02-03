#include <Cala/Scene.h>
#include <algorithm>
#include <Ende/thread/thread.h>
#include <Cala/Material.h>
#include <Cala/Probe.h>

cala::Scene::Scene(cala::Engine* engine, u32 count, u32 lightCount)
    : _engine(engine),
    _modelBuffer{engine->createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM),
                 engine->createBuffer(count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM)},
    _lightBuffer{engine->createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE),
                 engine->createBuffer(lightCount * sizeof(Light::Data), backend::BufferUsage::STORAGE)},
    _lightCountBuffer{engine->createBuffer(sizeof(u32) * 2 + sizeof(f32), backend::BufferUsage::STORAGE),
                 engine->createBuffer(sizeof(u32) * 2 + sizeof(f32), backend::BufferUsage::STORAGE)},
    _directionalLightCount(0)
{}


void cala::Scene::addRenderable(cala::Scene::Renderable &&renderable, cala::Transform *transform) {
    u32 key = 0;

    _renderables.push(std::make_pair(renderable, transform));
}

void cala::Scene::addRenderable(cala::Mesh &mesh, cala::MaterialInstance *materialInstance, cala::Transform *transform, bool castShadow) {
    if (mesh._index) {
        addRenderable(Renderable{
            mesh._vertex,
            (*mesh._index),
            materialInstance,
            {&mesh._binding, 1},
            mesh._attributes,
            castShadow
        }, transform);
    } else {
        addRenderable(Renderable{
            mesh._vertex,
            {},
            materialInstance,
            {&mesh._binding, 1},
            mesh._attributes,
            castShadow
        }, transform);
    }
}

u32 cala::Scene::addLight(cala::Light &light) {
    if (light.type() == Light::DIRECTIONAL)
        ++_directionalLightCount;
    _lights.push(std::move(light));
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
//    std::sort(_lights.begin(), _lights.end(), [](const Light& lhs, const Light& rhs) {
//        return !lhs.shadowing() and rhs.shadowing();
//    });
//
//    std::sort(_renderables.begin(), _renderables.end(), [](const std::pair<u32, std::pair<Renderable, Transform*>>& lhs, const std::pair<u32, std::pair<Renderable, Transform*>>& rhs) {
//        return lhs.first < rhs.first;
//    });

    camera.updateFrustum();

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

    std::sort(_renderList.begin(), _renderList.end(), [](const std::pair<u32, std::pair<Renderable, Transform*>>& lhs, const std::pair<u32, std::pair<Renderable, Transform*>>& rhs) {
        return lhs.first < rhs.first;
    });

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
        _lightBuffer[frame]->resize(_lightData.size() * sizeof(Light::Data) * 2);
    u32 lightCount[3] = { _directionalLightCount, static_cast<u32>(_lights.size() - _directionalLightCount), *reinterpret_cast<u32*>(&shadowBias) };
    _lightCountBuffer[frame]->data({ lightCount, sizeof(u32) * 3 });
    _lightBuffer[frame]->data({_lightData.data(), static_cast<u32>(_lightData.size() * sizeof(Light::Data))});

    _modelTransforms.clear();
    for (auto& renderablePair : _renderList) {
        auto& transform = renderablePair.second.second;
        _modelTransforms.push(transform->toMat());
    }
    if (_modelTransforms.size() * sizeof(ende::math::Mat4f) >= _modelBuffer[frame]->size())
        _modelBuffer[frame]->resize(_modelTransforms.size() * sizeof(ende::math::Mat4f) * 2);
    _modelBuffer[frame]->data({_modelTransforms.data(), static_cast<u32>(_modelTransforms.size() * sizeof(ende::math::Mat4f))});

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
                    if (materialInstance->getMaterial() != material) {
                        material = materialInstance->getMaterial();
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

            cmd.bindVertexBuffer(0, renderable.vertex.buffer().buffer());
            if (renderable.index)
                cmd.bindIndexBuffer(renderable.index.buffer());

            if (renderable.index)
                cmd.draw(renderable.index.size() / sizeof(u32), 1, 0, 0);
            else
                cmd.draw(renderable.vertex.size() / (4 * 14), 1, 0, 0);
        }
    }
}