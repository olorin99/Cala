#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/Probe.h>
#include <Cala/Material.h>
#include <Cala/ImGuiContext.h>
#include <Ende/profile/profile.h>

cala::Renderer::Renderer(cala::Engine* engine, cala::Renderer::Settings settings)
    : _engine(engine),
      _cameraBuffer{engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
                    engine->createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)},
      _drawCountBuffer{engine->createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
                       engine->createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)},
      _drawCommands{engine->createBuffer(sizeof(VkDrawIndexedIndirectCommand) * 100, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
                       engine->createBuffer(sizeof(VkDrawIndexedIndirectCommand) * 100, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)},
      _globalDataBuffer(engine->createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)),
      _graph(engine),
      _renderSettings(settings),
      _cullingFrustum(ende::math::perspective(45.f, 1920.f / 1080.f, 0.1f, 1000.f, true))
{
    _engine->driver().setBindlessSetIndex(0);

    _shadowTarget = _engine->createImage({
        1024, 1024, 1,
        backend::Format::D32_SFLOAT,
        1, 1,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT | backend::ImageUsage::TRANSFER_SRC
    });
    _engine->driver().immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto targetBarrier = _shadowTarget->barrier(backend::Access::NONE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);

        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::EARLY_FRAGMENT, 0, nullptr, { &targetBarrier, 1 });
    });
    backend::vulkan::RenderPass::Attachment shadowAttachment = {
            cala::backend::Format::D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    auto shadowRenderPass = _engine->driver().getRenderPass({ &shadowAttachment, 1 });
    u32 h = _shadowTarget.index();
    _shadowFramebuffer = _engine->driver().getFramebuffer(shadowRenderPass, { &_engine->getImageView(_shadowTarget).view, 1 }, { &h, 1 }, 1024, 1024);
}

bool cala::Renderer::beginFrame() {
    _frameInfo = _engine->driver().beginFrame();
    _engine->driver().waitFrame(_frameInfo.frame);
    _frameInfo.cmd->begin();
    _globalData.time = _engine->getRunningTime().milliseconds();
    auto mapped = _globalDataBuffer->map(0, sizeof(RendererGlobal));
    *static_cast<RendererGlobal*>(mapped.address) = _globalData;
    return true;
}

f64 cala::Renderer::endFrame() {
    _frameInfo.cmd->end();
    _frameInfo.cmd->submit({ &_frameInfo.swapchainInfo.imageAquired, 1 }, _frameInfo.fence);

    _engine->driver().swapchain().present(_frameInfo.swapchainInfo, _frameInfo.cmd->signal());
    _engine->driver().endFrame();

    _stats.pipelineCount = _frameInfo.cmd->pipelineCount();
    _stats.descriptorCount = _frameInfo.cmd->descriptorCount();
    _stats.drawCallCount = _frameInfo.cmd->drawCalls();

    return static_cast<f64>(_engine->driver().milliseconds()) / 1000.f;
}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera, ImGuiContext* imGui) {
    if (!_renderSettings.freezeFrustum)
        _cullingFrustum = camera.frustum();
    auto cameraData = camera.data();
    _cameraBuffer[frameIndex()]->data({ &cameraData, sizeof(cameraData) });
    {
        auto mapped = _drawCountBuffer[frameIndex()]->map(sizeof(u32), sizeof(u32));
        *static_cast<u32*>(mapped.address) = scene._renderables.size();
    }

    backend::vulkan::CommandBuffer& cmd = *_frameInfo.cmd;

    cmd.clearDescriptors();

    _graph.reset();

    ImageResource depthAttachment;
    depthAttachment.format = backend::Format::D32_SFLOAT;

    ImageResource pointDepth;
    pointDepth.format = backend::Format::D32_SFLOAT;
    pointDepth.matchSwapchain = false;
    pointDepth.transient = false;
    pointDepth.width = 10;
    pointDepth.height = 10;

    ImageResource colourAttachment;
    colourAttachment.format = backend::Format::RGBA32_SFLOAT;

    ImageResource backbufferAttachment;
    backbufferAttachment.format = backend::Format::RGBA8_UNORM;

    BufferResource transformsResource;
    transformsResource.handle = scene._modelBuffer[frameIndex()];
    transformsResource.size = scene._modelBuffer[frameIndex()]->size();
    transformsResource.usage = scene._modelBuffer[frameIndex()]->usage();

    BufferResource meshDataResource;
    meshDataResource.handle = scene._meshDataBuffer[frameIndex()];
    meshDataResource.size = scene._meshDataBuffer[frameIndex()]->size();
    meshDataResource.usage = scene._meshDataBuffer[frameIndex()]->usage();

    if (scene._renderables.size() * sizeof(VkDrawIndexedIndirectCommand) > _drawCommands[frameIndex()]->size())
        _drawCommands[frameIndex()] = _engine->resizeBuffer(_drawCommands[frameIndex()], scene._renderables.size() * sizeof(VkDrawIndexedIndirectCommand), false);

    BufferResource drawCommandsResource;
//    drawCommandsResource.size = sizeof(VkDrawIndexedIndirectCommand) * std::max(scene._renderables.size(), (u64)1);
    drawCommandsResource.size = _drawCommands[frameIndex()]->size();
//    drawCommandsResource.usage = backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT;
    drawCommandsResource.usage = _drawCommands[frameIndex()]->usage();
    drawCommandsResource.handle = _drawCommands[frameIndex()]; //TODO: have rendergraph automatically allocate double buffers for frames in flight

    _graph.setBackbuffer("backbuffer");

    auto& pointShadows = _graph.addPass("point_shadows");
    pointShadows.addImageOutput("point_depth", pointDepth);
    pointShadows.addBufferInput("transforms", transformsResource);
    pointShadows.addBufferInput("meshData", meshDataResource);

    pointShadows.addBufferOutput("drawCommands", drawCommandsResource);
    pointShadows.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto transforms = graph.getResource<BufferResource>("transforms");
        auto meshData = graph.getResource<BufferResource>("meshData");
        auto drawCommands = graph.getResource<BufferResource>("drawCommands");
        u32 shadowIndex = 0;
        for (u32 i = 0; i < scene._lights.size(); i++) {
            auto& light = scene._lights[i];
            if (light.shadowing() && light.type() == Light::LightType::POINT) {
                Transform shadowTransform(light.transform().pos());
                Camera shadowCam(ende::math::rad(90.f), 1024.f, 1024.f, light.getNear(), light.getFar(), shadowTransform);
                auto shadowMap = _engine->getShadowMap(shadowIndex++);

                for (u32 face = 0; face < 6; face++) {
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
                    shadowCam.updateFrustum();
                    ende::math::Frustum shadowFrustum = shadowCam.frustum();

                    cmd.clearDescriptors();
                    cmd.bindProgram(*_engine->_cullProgram);
                    cmd.bindBindings(nullptr);
                    cmd.bindAttributes(nullptr);
                    cmd.pushConstants({ &shadowFrustum, sizeof(shadowFrustum) });
                    cmd.bindBuffer(2, 0, *transforms->handle, true);
                    cmd.bindBuffer(2, 1, *meshData->handle, true);
                    cmd.bindBuffer(2, 2, *drawCommands->handle, true);
                    cmd.bindBuffer(2, 3, *_drawCountBuffer[frameIndex()], true);
                    cmd.bindPipeline();
                    cmd.bindDescriptors();
                    cmd.dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);


                    cmd.begin(*_shadowFramebuffer);

                    cmd.clearDescriptors();
                    cmd.bindRasterState({
                        backend::CullMode::FRONT
                    });
                    cmd.bindDepthState({
                        true, true,
                        backend::CompareOp::LESS_EQUAL
                    });

                    cmd.bindProgram(*_engine->_pointShadowProgram);

                    cmd.bindBuffer(3, 0, *scene._lightBuffer[frameIndex()], sizeof(Light::Data) * i, sizeof(Light::Data), true);

                    auto shadowData = shadowCam.data();
                    cmd.pushConstants({ &shadowData, sizeof(shadowData) });

                    auto& renderable = scene._renderables[0].second.first;
                    cmd.bindBindings(renderable.bindings);
                    cmd.bindAttributes(renderable.attributes);

                    cmd.bindBuffer(1, 0, *scene._modelBuffer[frameIndex()], true);

                    cmd.bindPipeline();
                    cmd.bindDescriptors();
                    cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
                    cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
                    cmd.drawIndirectCount(*drawCommands->handle, 0, *_drawCountBuffer[frameIndex()], 0, scene._renderables.size());

                    cmd.end(*_shadowFramebuffer);

                    auto srcBarrier = _shadowTarget->barrier(backend::Access::DEPTH_STENCIL_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT, backend::ImageLayout::TRANSFER_SRC);
                    auto dstBarrier = shadowMap->barrier(backend::Access::SHADER_READ, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY, backend::ImageLayout::TRANSFER_DST, face);
                    cmd.pipelineBarrier(backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::TRANSFER, 0, nullptr, { &srcBarrier, 1 });
                    cmd.pipelineBarrier(backend::PipelineStage::FRAGMENT_SHADER, backend::PipelineStage::TRANSFER, 0, nullptr, { &dstBarrier, 1 });

                    _shadowTarget->copy(cmd, *shadowMap, 0, face);
                    srcBarrier = _shadowTarget->barrier(backend::Access::TRANSFER_READ, backend::Access::DEPTH_STENCIL_WRITE, backend::ImageLayout::TRANSFER_SRC, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                    dstBarrier = shadowMap->barrier(backend::Access::TRANSFER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::TRANSFER_DST, backend::ImageLayout::SHADER_READ_ONLY, face);
                    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::EARLY_FRAGMENT, 0, nullptr, { &srcBarrier, 1 });
                    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &dstBarrier, 1 });
                }
            }
        }
    });

//    auto& directShadow = _graph.addPass("direct_shadow");
//    directShadow.addBufferInput("transforms");
//    directShadow.addBufferInput("meshData");
//    directShadow.addBufferOutput("drawCommands");
//
//    directShadow.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
//        for (u32 i = 0; i < scene._lights.size(); i++) {
//            auto& light = scene._lights[i];
//            if (light.shadowing() && light.type() == Light::LightType::DIRECTIONAL) {
//                f32 cascadeSplits[3];
//                f32 nearClip = light.getNear();
//                f32 farClip = light.getFar();
//                f32 clipRange = farClip - nearClip;
//
//                f32 minZ = nearClip;
//                f32 maxZ = nearClip + clipRange;
//                f32 range = maxZ - minZ;
//                f32 ratio = maxZ / minZ;
//
//                for (u32 j = 0; j < 3; j++) {
//                    f32 p = (j + 1) / 3.f;
//                    f32 log = minZ * std::pow(ratio, p);
//                    f32 uniform = minZ + range * p;
//                    f32 d = 0.95 * (log - uniform) + uniform;
//                    cascadeSplits[j] = (d - nearClip) / clipRange;
//                }
//
//                f32 lastSplitDist = 0.0;
//
//            }
//        }
//
//
//    });


    auto& cullPass = _graph.addPass("cull");
    cullPass.addBufferInput("transforms", transformsResource);
    cullPass.addBufferInput("meshData", meshDataResource);

    cullPass.addBufferOutput("drawCommands", drawCommandsResource);
    cullPass.setDebugColour({0.3, 0.3, 1, 1});

    cullPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto transforms = graph.getResource<BufferResource>("transforms");
        auto meshData = graph.getResource<BufferResource>("meshData");
        auto drawCommands = graph.getResource<BufferResource>("drawCommands");
        cmd.clearDescriptors();
        cmd.bindProgram(*_engine->_cullProgram);
        cmd.bindBindings(nullptr);
        cmd.bindAttributes(nullptr);
        cmd.pushConstants({ &_cullingFrustum, sizeof(_cullingFrustum) });
        cmd.bindBuffer(2, 0, *transforms->handle, true);
        cmd.bindBuffer(2, 1, *meshData->handle, true);
        cmd.bindBuffer(2, 2, *drawCommands->handle, true);
        cmd.bindBuffer(2, 3, *_drawCountBuffer[frameIndex()], true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);
    });


    if (_renderSettings.depthPre) {
        auto& depthPrePass = _graph.addPass("depth_pre");
        depthPrePass.setDepthOutput("depth", depthAttachment);
        depthPrePass.addBufferInput("drawCommands");

        depthPrePass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, *_cameraBuffer[frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindProgram(*_engine->_directShadowProgram);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindBuffer(4, 0, *scene._modelBuffer[frameIndex()], true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
            cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
            cmd.drawIndirectCount(*drawCommands->handle, 0, *_drawCountBuffer[frameIndex()], 0, scene._renderables.size());
        });
    }

    if (_renderSettings.forward) {
        auto& forwardPass = _graph.addPass("forward");
        if (_renderSettings.tonemap)
            forwardPass.addColourOutput("hdr", colourAttachment);
        else
            forwardPass.addColourOutput("backbuffer");
        if (_renderSettings.depthPre)
            forwardPass.setDepthInput("depth");
        else
            forwardPass.setDepthOutput("depth", depthAttachment);
        forwardPass.addImageInput("point_depth");
        forwardPass.addBufferInput("drawCommands");
        forwardPass.setDebugColour({0.4, 0.1, 0.9, 1});

        forwardPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, *_cameraBuffer[frameIndex()]);
//            cmd.bindBuffer(1, 1, *_globalDataBuffer);

            Material* material = nullptr;
            MaterialInstance* materialInstance = nullptr;

            if (!scene._lightData.empty()) {
                cmd.bindBuffer(3, 0, *scene._lightBuffer[frameIndex()], true);
                cmd.bindBuffer(3, 1, *scene._lightCountBuffer[frameIndex()], true);
            }

            if (scene._renderables.empty())
                return;
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            if (materialInstance != renderable.materialInstance) {
                materialInstance = renderable.materialInstance;
                if (materialInstance) {
                    if (materialInstance->material() != material) {
                        material = materialInstance->material();
                    }
                }
            }
            if (material) {
                cmd.bindProgram(*material->getProgram());
                cmd.bindRasterState(material->_rasterState);
                cmd.bindDepthState(material->_depthState);
            }

            cmd.bindBuffer(2, 0, *_engine->_materialBuffer, true);
            cmd.bindBuffer(2, 1, *meshData->handle, true);
            cmd.bindBuffer(4, 0, *scene._modelBuffer[frameIndex()], true);

            if (_renderSettings.ibl) {
                u32 IBLData[3] = { (u32)scene._skyLightIrradiance.index(), (u32)scene._skyLightPrefilter.index(), (u32)_engine->_brdfImage.index() };
                cmd.pushConstants({ IBLData, sizeof(u32) * 3 });
            } else {
                u32 IBLData[3] = { (u32)_engine->_defaultIrradiance.index(), (u32)_engine->_defaultPrefilter.index(), (u32)_engine->_brdfImage.index() };
                cmd.pushConstants({ IBLData, sizeof(u32) * 3 });
            }

            cmd.bindPipeline();
            cmd.bindDescriptors();

            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
            cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
            cmd.drawIndirectCount(*drawCommands->handle, 0, *_drawCountBuffer[frameIndex()], 0, scene._renderables.size());
        });
    }

    if (_renderSettings.tonemap) {
        auto& tonemapPass = _graph.addPass("tonemap");
        tonemapPass.addImageOutput("backbuffer", backbufferAttachment, true);
        tonemapPass.addImageInput("hdr", true);
        tonemapPass.setDebugColour({0.1, 0.4, 0.7, 1});
        tonemapPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto hdrImage = graph.getResource<ImageResource>("hdr");
            auto backbuffer = graph.getResource<ImageResource>("backbuffer");
            cmd.clearDescriptors();
            cmd.bindProgram(*_engine->_tonemapProgram);
            cmd.bindBindings(nullptr);
            cmd.bindAttributes(nullptr);
            cmd.bindBuffer(1, 0, *_cameraBuffer[frameIndex()]);
            cmd.bindBuffer(1, 1, *_globalDataBuffer);
            cmd.bindImage(2, 0, _engine->getImageView(hdrImage->handle), _engine->_defaultSampler, true);
            cmd.bindImage(2, 1, _engine->getImageView(backbuffer->handle), _engine->_defaultSampler, true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.dispatchCompute(std::ceil(backbuffer->width / 32.f), std::ceil(backbuffer->height / 32.f), 1);
        });
    }

    if (_renderSettings.skybox && scene._skyLightMap) {
        auto& skyboxPass = _graph.addPass("skybox");
        if (scene._hdrSkyLight && _renderSettings.tonemap)
            skyboxPass.addColourOutput("hdr");
        else
            skyboxPass.addColourOutput("backbuffer");
        skyboxPass.setDepthInput("depth");
        skyboxPass.setDebugColour({0, 0.7, 0.1, 1});

        skyboxPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            cmd.clearDescriptors();
            cmd.bindProgram(*_engine->_skyboxProgram);
            cmd.bindRasterState({ backend::CullMode::NONE });
            cmd.bindDepthState({ true, false, backend::CompareOp::LESS_EQUAL });
            cmd.bindBindings({ &_engine->_cube->_binding, 1 });
            cmd.bindAttributes(_engine->_cube->_attributes);
            cmd.bindBuffer(1, 0, *_cameraBuffer[frameIndex()]);
            cmd.bindImage(2, 0, scene._skyLightMapView, _engine->_defaultSampler);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
            cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
            cmd.draw(_engine->_cube->indexCount, 1, _engine->_cube->firstIndex, 0);
        });
    }

    if (imGui) {
        auto& uiPass = _graph.addPass("ui");
        uiPass.addColourOutput("backbuffer", backbufferAttachment);
        uiPass.setDebugColour({0.7, 0.1, 0.4, 1});

        uiPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            imGui->render(cmd);
        });
    }

    if (!_graph.compile())
        throw "cyclical graph found";

    cmd.startPipelineStatistics();
    _graph.execute(cmd, _frameInfo.swapchainInfo.index);
    cmd.stopPipelineStatistics();

}