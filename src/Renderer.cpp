#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/Probe.h>
#include <Cala/Material.h>
#include <Cala/ImGuiContext.h>

cala::Renderer::Renderer(cala::Engine* engine)
    : _engine(engine),
      _cameraBuffer(engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)),
      _lightCameraBuffer(engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT))
{
    _passTimers.emplace("totalGPU", backend::vulkan::Timer{_engine->driver(), 0});
    _passTimers.emplace("shadowsPass", backend::vulkan::Timer{_engine->driver(), 1});
    _passTimers.emplace("lightingPass", backend::vulkan::Timer{_engine->driver(), 2});

    _engine->driver().setBindlessSetIndex(0);
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

    _stats.pipelineCount = _frameInfo.cmd->pipelineCount();
    _stats.descriptorCount = _frameInfo.cmd->descriptorCount();
    _stats.drawCallCount = _frameInfo.cmd->drawCalls();

    return static_cast<f64>(_engine->driver().milliseconds()) / 1000.f;
}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera, ImGuiContext* imGui) {
    auto cameraData = camera.data();
    _cameraBuffer->data({ &cameraData, sizeof(cameraData) });

    backend::vulkan::CommandBuffer& cmd = *_frameInfo.cmd;
    cmd.startPipelineStatistics();
    _passTimers[0].second.start(cmd);
    cmd.pushDebugLabel("shadow pass");

    // shadows
    _passTimers[1].second.start(cmd);
    for (u32 light = 0; light < scene._lights.size(); ++light) {
        if (scene._lights[light].shadowing()) {
            // draw shadows
            auto& lightObj = scene._lights[light];

            Transform shadowTransform(lightObj.transform().pos());
            Camera shadowCamera((f32)ende::math::rad(90.f), 1024.f, 1024.f, lightObj.getNear(), lightObj.getFar(), shadowTransform);

            if (lightObj.type() == Light::POINT) { // point shadows
                auto& shadowProbe = _engine->getShadowProbe(light);
                shadowProbe.draw(cmd, [&](backend::vulkan::CommandBuffer& cmd, u32 face) {
                    cmd.clearDescriptors();
                    cmd.bindRasterState({
                        backend::CullMode::FRONT,
                    });
                    cmd.bindDepthState({
                        true, true,
                        backend::CompareOp::LESS_EQUAL
                    });

                    cmd.bindProgram(*_engine->_pointShadowProgram);

                    cmd.bindBuffer(3, 0, *scene._lightBuffer[frameIndex()], light * sizeof(Light::Data), sizeof(Light::Data));

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

                    for (u32 i = 0; i < scene._renderList.size(); ++i) {
                        auto& renderable = scene._renderList[i].second.first;
                        auto& transform = scene._renderList[i].second.second;

                        if (!renderable.castShadows)
                            continue;

                        if (!shadowCamera.frustum().intersect(transform->pos(), 2))
                            continue;

                        cmd.bindBindings(renderable.bindings);
                        cmd.bindAttributes(renderable.attributes);
                        cmd.bindBuffer(1, 0, *scene._modelBuffer[frameIndex()], i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));
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
    cmd.popDebugLabel();
    _passTimers[1].second.stop();


    cmd.clearDescriptors();

    _passTimers[2].second.start(cmd);
    cmd.pushDebugLabel("lighting pass");
    cmd.begin(*_swapchainInfo.framebuffer);

    cmd.bindBuffer(1, 0, *_cameraBuffer);

    Material* material = nullptr;
    MaterialInstance* materialInstance = nullptr;

//    cmd.pushDebugLabel("direction_lights", {0, 0, 1, 1});

    if (!scene._lightData.empty()) {
//        cmd.bindBuffer(3, 0, *scene._lightBuffer[frameIndex()], 0, sizeof(Light::Data) * scene._directionalLightCount);
        cmd.bindBuffer(3, 0, *scene._lightBuffer[frameIndex()]);
//        cmd.bindBuffer(3, 1, *scene._lightCountBuffer[frameIndex()], 0, sizeof(u32));
        cmd.bindBuffer(3, 1, *scene._lightCountBuffer[frameIndex()]);
//        auto lightCamData = scene._lights[0].camera().data();
//        _lightCameraBuffer->data({ &lightCamData, sizeof(lightCamData) });
//        cmd.bindBuffer(0, 1, *_lightCameraBuffer);

//            if (scene._lights[light].type() == Light::DIRECTIONAL)
//                variant = Material::Variants::DIRECTIONAL;
    }

    // lights
    for (u32 i = 0; i < scene._renderList.size(); ++i) {
        auto& renderable = scene._renderList[i].second.first;
        auto& transform = scene._renderList[i].second.second;

        if (!camera.frustum().intersect(transform->pos(), transform->scale().x()))
            continue;

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
            cmd.bindProgram(*material->getProgram(Material::Variants::POINT));
            cmd.bindRasterState(material->_rasterState);
            cmd.bindDepthState(material->_depthState);
        }

        cmd.bindBuffer(4, 0, *scene._modelBuffer[frameIndex()], i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

//        if (scene._lights[0].shadowing())
//            cmd.bindImage(2, 5, _engine->getShadowProbe(0).view(), _engine->_shadowSampler); // TODO: directional shadows
//        else
//            cmd.bindImage(2, 5, _engine->_defaultPointShadowView, _engine->_shadowSampler);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.bindVertexBuffer(0, renderable.vertex.buffer().buffer());
        if (renderable.index) {
            cmd.bindIndexBuffer(renderable.index.buffer());
            cmd.draw(renderable.index.size() / sizeof(u32), 1, 0, 0);
        } else
            cmd.draw(renderable.vertex.size() / (4 * 14), 1, 0, 0);
    }

    if (scene._skyLightMap) {
        cmd.clearDescriptors();
        cmd.bindProgram(*_engine->_skyboxProgram);
        cmd.bindRasterState({ backend::CullMode::NONE });
        cmd.bindDepthState({ true, false, backend::CompareOp::LESS_EQUAL });
        cmd.bindBindings({ &_engine->_cube._binding, 1 });
        cmd.bindAttributes(_engine->_cube._attributes);
        cmd.bindBuffer(1, 0, *_cameraBuffer);
        cmd.bindImage(2, 0, scene._skyLightMapView, _engine->_defaultSampler);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.bindVertexBuffer(0, _engine->_cube._vertex.buffer());
        cmd.bindIndexBuffer(*_engine->_cube._index);
        cmd.draw(_engine->_cube._index->size() / sizeof(u32), 1, 0, 0);
    }

    cmd.popDebugLabel();
    _passTimers[2].second.stop();
    cmd.stopPipelineStatistics();

    cmd.pushDebugLabel("gui pass");
    if (imGui)
        imGui->render(cmd);
    cmd.popDebugLabel();

    cmd.end(*_swapchainInfo.framebuffer);
    _passTimers[0].second.stop();

}