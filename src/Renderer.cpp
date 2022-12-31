#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/Probe.h>
#include <Cala/ImGuiContext.h>

cala::Renderer::Renderer(cala::Engine* engine)
    : _engine(engine),
      _cameraBuffer(engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)),
      _lightCameraBuffer(engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT))
{
    _passTimers.emplace("shadowsPass", backend::vulkan::Timer{_engine->driver(), 0});
    _passTimers.emplace("lightingPass", backend::vulkan::Timer{_engine->driver(), 1});
}

bool cala::Renderer::beginFrame() {
    _frameInfo = _engine->driver().beginFrame();
    _engine->driver().waitFrame(_frameInfo.frame);
    _swapchainInfo = _engine->driver().swapchain().nextImage();
    _frameInfo.cmd->begin();
    return true;
}

f64 cala::Renderer::endFrame() {

//    _frameInfo.cmd->end(*_swapchainInfo.framebuffer);
    _frameInfo.cmd->end();
    _frameInfo.cmd->submit({ &_swapchainInfo.imageAquired, 1 }, _frameInfo.fence);

    _engine->driver().swapchain().present(_swapchainInfo, _frameInfo.cmd->signal());
    _engine->driver().endFrame();

    return static_cast<f64>(_engine->driver().milliseconds()) / 1000.f;
}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera, ImGuiContext* imGui) {
    camera.updateFrustum();
    auto cameraData = camera.data();
    _cameraBuffer->data({ &cameraData, sizeof(cameraData) });

    backend::vulkan::CommandBuffer& cmd = *_frameInfo.cmd;

    // shadows
    _passTimers[0].second.start(cmd);
    for (u32 light = 0; light < scene._lights.size(); ++light) {
        if (scene._lights[light].shadowing()) {
            // draw shadows
            auto& lightObj = scene._lights[light];

            Transform shadowTransform(lightObj.transform().pos());
            Camera shadowCamera((f32)ende::math::rad(90.f), 1024.f, 1024.f, 0.1f, 100.f, shadowTransform);

            if (lightObj.type() == Light::POINT) { // point shadows
                auto& shadowProbe = _engine->getShadowProbe(light);
                shadowProbe.draw(cmd, [&](backend::vulkan::CommandBuffer& cmd, u32 face) {
                    cmd.clearDescriptors();
                    cmd.bindRasterState({
                        backend::CullMode::FRONT
                    });
                    cmd.bindDepthState({
                        true, true,
                        backend::CompareOp::LESS_EQUAL
                    });

                    cmd.bindProgram(*_engine->_pointShadowProgram);

                    cmd.bindBuffer(3, 0, scene._lightBuffer, light * sizeof(Light::Data), sizeof(Light::Data));

                    switch (face) {
                        case 0:
                            shadowTransform.rotate({0, 1, 0}, ende::math::rad(90));
                            break;
                        case 1:
                            shadowTransform.rotate({0, 1, 0}, ende::math::rad(180));
                            break;
                        case 2:
                            shadowTransform.rotate({0, 1, 0}, ende::math::rad(90));
                            shadowTransform.rotate({1, 0, 0}, ende::math::rad(90));
                            break;
                        case 3:
                            shadowTransform.rotate({1, 0, 0}, ende::math::rad(180));
                            break;
                        case 4:
                            shadowTransform.rotate({1, 0, 0}, ende::math::rad(90));
                            break;
                        case 5:
                            shadowTransform.rotate({0, 1, 0}, ende::math::rad(180));
                            break;
                    }

                    auto shadowData = shadowCamera.data();
                    cmd.pushConstants({ &shadowData, sizeof(shadowData) });
                    shadowCamera.updateFrustum();

                    for (u32 i = 0; i < scene._renderables.size(); ++i) {
                        auto& renderable = scene._renderables[i].second.first;
                        auto& transform = scene._renderables[i].second.second;

                        if (!shadowCamera.frustum().intersect(transform->pos(), 2))
                            continue;

                        cmd.bindBindings(renderable.bindings);
                        cmd.bindAttributes(renderable.attributes);
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
                });
            } else { // directional shadows

            }

        }
    }
    _passTimers[0].second.stop();

    cmd.clearDescriptors();

    _passTimers[1].second.start(cmd);
    cmd.begin(*_swapchainInfo.framebuffer);

    cmd.bindBuffer(0, 0, *_cameraBuffer);

    MaterialInstance* materialInstance = nullptr;

    u32 lightCount = scene._lightData.empty() ? 1 : scene._lightData.size(); // if no lights present draw scene once
    for (u32 light = 0; light < lightCount; ++light) {

        if (!scene._lightData.empty()) {
            cmd.bindBuffer(3, 0, scene._lightBuffer, light * sizeof(Light::Data), sizeof(Light::Data));
            auto lightCamData = scene._lights[light].camera().data();
            _lightCameraBuffer->data({ &lightCamData, sizeof(lightCamData) });
            cmd.bindBuffer(0, 1, *_lightCameraBuffer);
        }

        for (u32 i = 0; i < scene._renderables.size(); ++i) {
            auto& renderable = scene._renderables[i].second.first;
            auto& transform = scene._renderables[i].second.second;

            if (!camera.frustum().intersect(transform->pos(), transform->scale().x()))
                continue;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            if (materialInstance != renderable.materialInstance) {
                materialInstance = renderable.materialInstance;
                if (materialInstance)
                    materialInstance->bind(cmd);
            }

            cmd.bindBuffer(1, 0, scene._modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

            cmd.bindImage(2, 3, _engine->getShadowProbe(light).view(), _engine->_defaultSampler);

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
    _passTimers[1].second.stop();

    if (imGui)
        imGui->render(cmd);

    cmd.end(*_swapchainInfo.framebuffer);

}