#include <Cala/Scene.h>
#include <algorithm>

cala::Scene::Scene(backend::vulkan::Driver &driver, u32 count, u32 lightCount)
    : _modelBuffer(driver, count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM),
      _lightBuffer(driver, lightCount * sizeof(Light::Data), backend::BufferUsage::UNIFORM)
{}


void cala::Scene::addRenderable(cala::Scene::Renderable &&renderable, cala::Transform *transform) {
    u32 key = 0;

    _renderables.push(std::make_pair(key, std::make_pair(renderable, transform)));
}

void cala::Scene::addRenderable(cala::Mesh &mesh, cala::MaterialInstance *materialInstance, cala::Transform *transform, bool castShadow) {
    if (mesh._index) {
        addRenderable(Renderable{
            mesh._vertex,
            (*mesh._index),
            materialInstance,
            {&mesh._binding, 1},
            mesh._attributes
        }, transform);
    } else {
        addRenderable(Renderable{
            mesh._vertex,
            {},
            materialInstance,
            {&mesh._binding, 1},
            mesh._attributes
        }, transform);
    }
}

void cala::Scene::addLight(cala::Light &light) {
    _lights.push(std::move(light));
}


void cala::Scene::prepare(cala::Camera& camera) {
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
        auto viewDir = camera.transform().pos() - item.second.second->pos();
        f32 depth = viewDir.length();
        item.first = *reinterpret_cast<u32*>(&depth);
        _renderList.push(item);
    }
    std::sort(_renderables.begin(), _renderables.end(), [](const std::pair<u32, std::pair<Renderable, Transform*>>& lhs, const std::pair<u32, std::pair<Renderable, Transform*>>& rhs) {
        return lhs.first < rhs.first;
    });


    _lightData.clear();
    for (auto& light : _lights) {
        _lightData.push(light.data());
    }
    _lightBuffer.data({_lightData.data(), static_cast<u32>(_lightData.size() * sizeof(Light::Data))});

    _modelTransforms.clear();

    for (auto& renderablePair : _renderList) {
        auto& transform = renderablePair.second.second;
        _modelTransforms.push(transform->toMat());
    }


    if (_modelTransforms.size() * sizeof(ende::math::Mat4f) >= _modelBuffer.size())
        _modelBuffer.resize(_modelTransforms.size() * sizeof(ende::math::Mat4f) * 2);
    _modelBuffer.data({_modelTransforms.data(), static_cast<u32>(_modelTransforms.size() * sizeof(ende::math::Mat4f))});
}

void cala::Scene::render(backend::vulkan::CommandBuffer& cmd) {

    MaterialInstance* materialInstance = nullptr;

    u32 lightCount = _lightData.empty() ? 1 : _lightData.size();

    for (u32 light = 0; light < lightCount; light++) {
        if (!_lightData.empty())
            cmd.bindBuffer(3, 0, _lightBuffer, light * sizeof(Light::Data), sizeof(Light::Data));

        for (u32 i = 0; i < _renderables.size(); i++) {
            auto& renderable = _renderables[i].second.first;
            auto& transform = _renderables[i].second.second;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            if (materialInstance != renderable.materialInstance) {
                materialInstance = renderable.materialInstance;
                renderable.materialInstance->bind(cmd);
            }

            cmd.bindBuffer(1, 0, _modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

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