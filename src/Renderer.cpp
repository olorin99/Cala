#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Driver.h>

cala::Renderer::Renderer(cala::Engine* engine)
    : _engine(engine),
      _cameraBuffer(engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT))
{}

bool cala::Renderer::beginFrame() {
    _frameInfo = _engine->driver().beginFrame();
    _engine->driver().waitFrame(_frameInfo.frame);
    _swapchainInfo = _engine->driver().swapchain().nextImage();
    return true;
}

f64 cala::Renderer::endFrame() {
    _frameInfo.cmd->submit({ &_swapchainInfo.imageAquired, 1 }, _frameInfo.fence);

    _engine->driver().swapchain().present(_swapchainInfo, _frameInfo.cmd->signal());
    _engine->driver().endFrame();

    return static_cast<f64>(_engine->driver().milliseconds()) / 1000.f;
}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera) {
    auto cameraData = camera.data();
    _cameraBuffer->data({ &cameraData, sizeof(cameraData) });

    backend::vulkan::CommandBuffer& cmd = *_frameInfo.cmd;


    cmd.begin();
    cmd.begin(*_swapchainInfo.framebuffer);

    cmd.bindBuffer(0, 0, *_cameraBuffer);

    MaterialInstance* materialInstance = nullptr;

    for (u32 light = 0; light < scene._lights.size(); ++light) {

        if (!scene._lightData.empty())
            cmd.bindBuffer(3, 0, scene._lightBuffer, light * sizeof(Light::Data), sizeof(Light::Data));

        for (u32 i = 0; i < scene._renderables.size(); ++i) {
            auto& renderable = scene._renderables[i].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            if (materialInstance != renderable.materialInstance) {
                materialInstance = renderable.materialInstance;
                if (materialInstance)
                    materialInstance->bind(cmd);
            }

            cmd.bindBuffer(1, 0, scene._modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

            cmd.bindPipeline();
            cmd.bindDescriptors();

            cmd.bindVertexBuffer(0, renderable.vertex.buffer().buffer());
            if (renderable.index) {
                cmd.bindIndexBuffer(renderable.index.buffer());
                cmd.draw(renderable.index.size() / sizeof(u32), 1, 0, 0);
            } else
                cmd.draw(renderable.vertex.size() / (4 * 14), 1, 0, 0);
        }

    }

    cmd.end(*_swapchainInfo.framebuffer);
    cmd.end();



//    auto frame = _driver.swapchain().nextImage();
//    backend::vulkan::CommandBuffer* cmd = _driver.beginFrame();
//
//    cmd->begin(frame.framebuffer);
//
//    cmd->bindBuffer(0, 0, _cameraBuffer);
//
//    scene.render(*cmd);
//
//    cmd->end(frame.framebuffer);
//
//    cmd->submit({ &frame.imageAquired, 1 }, frame.fence);
//    _driver.endFrame();

}