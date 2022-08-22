#include <Cala/Scene.h>

cala::Scene::Scene(backend::vulkan::Driver &driver, u32 count)
    : _modelBuffer(driver, count * sizeof(ende::math::Mat4f), backend::BufferUsage::UNIFORM)
{}


void cala::Scene::addRenderable(cala::Scene::Renderable &&renderable, cala::Transform *transform) {
    _renderables.push(std::make_pair(std::move(renderable), transform));
}


void cala::Scene::prepare() {
    ende::Vector<ende::math::Mat4f> _modelTransforms; //TODO: preallocate memory

    for (auto& renderablePair : _renderables) {
        _modelTransforms.push(renderablePair.second->toMat());
    }

    _modelBuffer.data({_modelTransforms.data(), static_cast<u32>(_modelTransforms.size() * sizeof(ende::math::Mat4f))});
}

void cala::Scene::render(backend::vulkan::CommandBuffer& cmd) {
    for (u32 i = 0; i < _renderables.size(); i++) {
        auto& renderable = _renderables[i];

        cmd.bindBindings(renderable.first.bindings);
        cmd.bindAttributes(renderable.first.attributes);

        renderable.first.materialInstance->bind(cmd);

        cmd.bindBuffer(1, 0, _modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.bindVertexBuffer(0, renderable.first.vertex->buffer());
        if (renderable.first.index)
            cmd.bindIndexBuffer(*renderable.first.index);

        if (renderable.first.index)
            cmd.draw(renderable.first.index->size() / sizeof(u32), 1, 0, 0);
        else
            cmd.draw(renderable.first.vertex->size() / (4 * 14), 1, 0, 0);
    }
}