#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/Probe.h>
#include <Cala/Material.h>
#include <Cala/ImGuiContext.h>
#include <Ende/profile/profile.h>

cala::Renderer::Renderer(cala::Engine* engine, cala::Renderer::Settings settings)
    : _engine(engine),
    _swapchain(nullptr),
    _cameraBuffer{engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
                  engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)},
    _drawCountBuffer{engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
                     engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)},
    _drawCommands{engine->device().createBuffer(sizeof(VkDrawIndexedIndirectCommand) * 100, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
                  engine->device().createBuffer(sizeof(VkDrawIndexedIndirectCommand) * 100, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)},
    _globalDataBuffer(engine->device().createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)),
    _graph(engine),
    _renderSettings(settings),
    _cullingFrustum(ende::math::perspective(45.f, 1920.f / 1080.f, 0.1f, 1000.f, true))
{
    _engine->device().setBindlessSetIndex(0);

    _shadowTarget = _engine->device().createImage({
        1024, 1024, 1,
        backend::Format::D32_SFLOAT,
        1, 1,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT | backend::ImageUsage::TRANSFER_SRC
    });
    _engine->device().immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto targetBarrier = _shadowTarget->barrier(backend::Access::NONE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);

        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::EARLY_FRAGMENT, { &targetBarrier, 1 });
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
    auto shadowRenderPass = _engine->device().getRenderPass({&shadowAttachment, 1 });
    u32 h = _shadowTarget.index();
    _shadowFramebuffer = _engine->device().getFramebuffer(shadowRenderPass, {&_engine->device().getImageView(_shadowTarget).view, 1 }, {&h, 1 }, 1024, 1024);
}

bool cala::Renderer::beginFrame(cala::backend::vulkan::Swapchain* swapchain) {
    _swapchain = swapchain;
    assert(_swapchain);
    _frameInfo = _engine->device().beginFrame();
    _swapchainFrame = _swapchain->nextImage();
    _frameInfo.cmd->begin();
    _globalData.time = _engine->getRunningTime().milliseconds();
    auto mapped = _globalDataBuffer->map(0, sizeof(RendererGlobal));
    *static_cast<RendererGlobal*>(mapped.address) = _globalData;
    return true;
}

f64 cala::Renderer::endFrame() {
    _frameInfo.cmd->end();
    _frameInfo.cmd->submit({ &_swapchainFrame.imageAquired, 1 }, _frameInfo.fence);

    assert(_swapchain);
    _swapchain->present(_swapchainFrame, _frameInfo.cmd->signal());
    _engine->device().endFrame();

    _stats.drawCallCount = _frameInfo.cmd->drawCalls();

    return static_cast<f64>(_engine->device().milliseconds()) / 1000.f;
}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera, ImGuiContext* imGui) {
    PROFILE_NAMED("Renderer::render");
    if (!_renderSettings.freezeFrustum)
        _cullingFrustum = camera.frustum();
    auto cameraData = camera.data();
    _cameraBuffer[_engine->device().frameIndex()]->data({ &cameraData, sizeof(cameraData) });
    {
        auto mapped = _drawCountBuffer[_engine->device().frameIndex()]->map(sizeof(u32), sizeof(u32));
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
    transformsResource.handle = scene._modelBuffer[_engine->device().frameIndex()];
    transformsResource.size = scene._modelBuffer[_engine->device().frameIndex()]->size();
    transformsResource.usage = scene._modelBuffer[_engine->device().frameIndex()]->usage();

    BufferResource meshDataResource;
    meshDataResource.handle = scene._meshDataBuffer[_engine->device().frameIndex()];
    meshDataResource.size = scene._meshDataBuffer[_engine->device().frameIndex()]->size();
    meshDataResource.usage = scene._meshDataBuffer[_engine->device().frameIndex()]->usage();

    BufferResource materialCountResource;
    materialCountResource.handle = scene._materialCountBuffer[_engine->device().frameIndex()];
//    materialCountResource.transient = false;
    materialCountResource.size = scene._materialCountBuffer[_engine->device().frameIndex()]->size();
    materialCountResource.usage = scene._materialCountBuffer[_engine->device().frameIndex()]->usage();

    if (scene._renderables.size() * sizeof(VkDrawIndexedIndirectCommand) > _drawCommands[_engine->device().frameIndex()]->size())
        _drawCommands[_engine->device().frameIndex()] = _engine->device().resizeBuffer(_drawCommands[_engine->device().frameIndex()], scene._renderables.size() * sizeof(VkDrawIndexedIndirectCommand), false);

    BufferResource drawCommandsResource;
    drawCommandsResource.size = _drawCommands[_engine->device().frameIndex()]->size();
    drawCommandsResource.transient = false;
    drawCommandsResource.usage = _drawCommands[_engine->device().frameIndex()]->usage();
    drawCommandsResource.handle = _drawCommands[_engine->device().frameIndex()]; //TODO: have rendergraph automatically allocate double buffers for frames in flight

    BufferResource clustersResource;
    clustersResource.size = sizeof(ende::math::Vec4f) * 2 * 16 * 9 * 24;
    clustersResource.usage = backend::BufferUsage::STORAGE;
    clustersResource.transient = false;

    _graph.setBackbuffer("backbuffer");

    if (camera.isDirty()) {
        auto& createClusters = _graph.addPass("create_clusters");
        createClusters.addBufferOutput("clusters", clustersResource);

        createClusters.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto clusters = graph.getResource<BufferResource>("clusters");
            cmd.clearDescriptors();
            cmd.bindProgram(*_engine->_createClustersProgram);
            cmd.bindBindings(nullptr);
            cmd.bindAttributes(nullptr);
            struct ClusterPush {
                ende::math::Mat4f inverseProjection;
                ende::math::Vec<4, u32> tileSizes;
                ende::math::Vec<2, u32> screenSize;
                f32 near;
                f32 far;
            } push;
            push.inverseProjection = camera.projection().inverse();
            push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
            push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
            push.near = camera.near();
            push.far = camera.far();

            cmd.pushConstants({ &push, sizeof(push) });
            cmd.bindBuffer(1, 0, *clusters->handle, true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.dispatchCompute(16, 9, 24);
        });
        camera.setDirty(false);
    }

    BufferResource lightGridResource;
    lightGridResource.size = sizeof(u32) * 2 * 16 * 9 * 24;
    lightGridResource.usage = backend::BufferUsage::STORAGE;
    lightGridResource.transient = false;
    BufferResource lightIndicesResource;
    lightIndicesResource.size = sizeof(u32) * 16 * 9 * 24 * 100;
    lightIndicesResource.usage = backend::BufferUsage::STORAGE;
    lightIndicesResource.transient = false;
    BufferResource lightGlobalIndexResource;
    lightGlobalIndexResource.size = sizeof(u32);
    lightGlobalIndexResource.transient = false;
    lightGlobalIndexResource.usage = backend::BufferUsage::STORAGE;

    auto& cullLights = _graph.addPass("cull_lights");
    cullLights.addBufferInput("clusters", clustersResource);
    cullLights.addBufferOutput("lightGrid", lightGridResource);
    cullLights.addBufferOutput("lightIndices", lightIndicesResource);
    cullLights.addBufferOutput("lightGlobalResource", lightGlobalIndexResource);

    cullLights.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto clusters = graph.getResource<BufferResource>("clusters");
        auto lightGrid = graph.getResource<BufferResource>("lightGrid");
        auto lightIndices = graph.getResource<BufferResource>("lightIndices");
        auto lightGlobalIndex = graph.getResource<BufferResource>("lightGlobalResource");
        cmd.clearDescriptors();
        cmd.bindProgram(*_engine->_cullLightsProgram);
        cmd.bindBindings(nullptr);
        cmd.bindAttributes(nullptr);
        struct ClusterPush {
            ende::math::Mat4f inverseProjection;
            ende::math::Vec<4, u32> tileSizes;
            ende::math::Vec<2, u32> screenSize;
            f32 near;
            f32 far;
        } push;
        push.inverseProjection = camera.projection().inverse();
        push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
        push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
        push.near = camera.near();
        push.far = camera.far();

        cmd.pushConstants({ &push, sizeof(push) });
        cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()], false);
        cmd.bindBuffer(1, 1, *clusters->handle, true);
        cmd.bindBuffer(1, 2, *scene._lightBuffer[_engine->device().frameIndex()], true);
        cmd.bindBuffer(1, 3, *lightGrid->handle, true);
        cmd.bindBuffer(1, 4, *lightGlobalIndex->handle, true);
        cmd.bindBuffer(1, 5, *lightIndices->handle, true);
        cmd.bindBuffer(1, 6, *scene._lightCountBuffer[_engine->device().frameIndex()], true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(1, 1, 6);
    });

    if (_renderSettings.debugClusters) {
        auto& debugClusters = _graph.addPass("debug_clusters");

        debugClusters.addBufferInput("lightGrid");
        debugClusters.addColourOutput("hdr");
        debugClusters.addImageInput("depth");

        debugClusters.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto lightGrid = graph.getResource<BufferResource>("lightGrid");
            auto depthBuffer = graph.getResource<ImageResource>("depth");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()]);
            cmd.bindBindings(nullptr);
            cmd.bindAttributes(nullptr);
            cmd.bindBlendState({ true });
            cmd.bindProgram(*_engine->_clusterDebugProgram);
            struct ClusterPush {
                ende::math::Vec<4, u32> tileSizes;
                ende::math::Vec<2, u32> screenSize;
            } push;
            push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
            push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
            cmd.pushConstants({ &push, sizeof(push) });
            cmd.bindBuffer(1, 1, *lightGrid->handle, true);
            cmd.bindImage(1, 2, _engine->device().getImageView(depthBuffer->handle), _engine->device().defaultShadowSampler());
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.draw(3, 1, 0, 0, false);
            cmd.bindBlendState({ false });
        });
    }

    if (_renderSettings.debugNormals) {
        auto& debugNormals = _graph.addPass("debug_normals");
        debugNormals.addColourOutput("backbuffer");
        debugNormals.setDepthOutput("depth", depthAttachment);
        debugNormals.addBufferInput("drawCommands");

        debugNormals.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindProgram(*_engine->_normalsDebugProgram);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({});
            cmd.bindBuffer(2, 1, *meshData->handle, true);
            cmd.bindBuffer(4, 0, *scene._modelBuffer[_engine->device().frameIndex()], true);
            cmd.bindPipeline();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
            cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                cmd.bindBuffer(2, 0, *_engine->_materials[material].buffer(), true);
                cmd.bindDescriptors();
                cmd.drawIndirectCount(*drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), *scene._materialCountBuffer[_engine->device().frameIndex()], material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }


    auto& pointShadows = _graph.addPass("point_shadows");
    pointShadows.addImageOutput("point_depth", pointDepth);
    pointShadows.addBufferInput("transforms", transformsResource);
    pointShadows.addBufferInput("meshData", meshDataResource);
    pointShadows.addBufferInput("materialCounts", materialCountResource);

    pointShadows.addBufferOutput("drawCommands", drawCommandsResource);
    pointShadows.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto transforms = graph.getResource<BufferResource>("transforms");
        auto meshData = graph.getResource<BufferResource>("meshData");
        auto materialCounts = graph.getResource<BufferResource>("materialCounts");
        auto drawCommands = graph.getResource<BufferResource>("drawCommands");
        u32 shadowIndex = 0;
        for (u32 i = 0; i < scene._lights.size(); i++) {
            auto& light = scene._lights[i].second;
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
                    cmd.bindProgram(*_engine->_pointShadowCullProgram);
                    cmd.bindBindings(nullptr);
                    cmd.bindAttributes(nullptr);
                    cmd.pushConstants({ &shadowFrustum, sizeof(shadowFrustum) });
                    cmd.bindBuffer(2, 0, *transforms->handle, true);
                    cmd.bindBuffer(2, 1, *meshData->handle, true);
                    cmd.bindBuffer(2, 2, *drawCommands->handle, true);
                    cmd.bindBuffer(2, 3, *_drawCountBuffer[_engine->device().frameIndex()], true);
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

                    cmd.bindBuffer(3, 0, *scene._lightBuffer[_engine->device().frameIndex()], sizeof(Light::Data) * i, sizeof(Light::Data), true);

                    struct ShadowData {
                        ende::math::Mat4f viewProjection;
                        ende::math::Vec3f position;
                        f32 near;
                        f32 far;
                    };
                    ShadowData shadowData {
                        shadowCam.viewProjection(),
                        shadowCam.transform().pos(),
                        shadowCam.near(),
                        shadowCam.far()
                    };
                    cmd.pushConstants({ &shadowData, sizeof(shadowData) });

                    auto& renderable = scene._renderables[0].second.first;
                    cmd.bindBindings(renderable.bindings);
                    cmd.bindAttributes(renderable.attributes);

                    cmd.bindBuffer(1, 0, *scene._modelBuffer[_engine->device().frameIndex()], true);

                    cmd.bindPipeline();
                    cmd.bindDescriptors();
                    cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
                    cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
                    cmd.drawIndirectCount(*drawCommands->handle, 0, *_drawCountBuffer[_engine->device().frameIndex()], 0, scene._renderables.size());

                    cmd.end(*_shadowFramebuffer);

                    auto srcBarrier = _shadowTarget->barrier(backend::Access::DEPTH_STENCIL_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::TRANSFER_SRC);
                    auto dstBarrier = shadowMap->barrier(backend::Access::SHADER_READ, backend::Access::TRANSFER_WRITE, backend::ImageLayout::TRANSFER_DST, face);
                    cmd.pipelineBarrier(backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::TRANSFER, { &srcBarrier, 1 });
                    cmd.pipelineBarrier(backend::PipelineStage::FRAGMENT_SHADER, backend::PipelineStage::TRANSFER, { &dstBarrier, 1 });

                    _shadowTarget->copy(cmd, *shadowMap, 0, face);
                    srcBarrier = _shadowTarget->barrier(backend::Access::TRANSFER_READ, backend::Access::DEPTH_STENCIL_WRITE, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                    dstBarrier = shadowMap->barrier(backend::Access::TRANSFER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY, face);
                    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::EARLY_FRAGMENT, { &srcBarrier, 1 });
                    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::FRAGMENT_SHADER, { &dstBarrier, 1 });
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
//                for (u32 j = 0; j < 3; j++) {
//                    f32 splitDist = cascadeSplits[j];
//                    ende::math::Vec3f frustumCorners[8] = {
//                            {-1, 1, -1},
//                            {1, 1, -1},
//                            {1, -1, -1},
//                            {-1, -1, -1},
//                            {-1, 1, 1},
//                            {1, 1, 1},
//                            {1, -1, 1},
//                            {-1, -1, 1}
//                    };
//                    auto camInv = camera.viewProjection().inverse();
//                    for (u32 k = 0; k < 4; k++) {
//                        auto dist = frustumCorners[k + 4] - frustumCorners[k];
//                        frustumCorners[k + 4] = frustumCorners[k] + (dist * splitDist);
//                        frustumCorners[k] = frustumCorners[k] + (dist * lastSplitDist);
//                    }
//
//                    ende::math::Vec3f frustumCenter{0, 0, 0};
//                    for (u32 k = 0; k < 8; k++) {
//                        frustumCenter = frustumCenter + frustumCorners[k];
//                    }
//                    frustumCenter = frustumCenter / 8.f;
//                    f32 radius = 0.;
//                    for (u32 k = 0; k < 8; k++) {
//                        f32 distance = (frustumCorners[k] - frustumCenter).length();
//                        radius = std::max(radius, distance);
//                    }
//                    radius = std::ceil(radius * 16) / 16;
//                    ende::math::Vec3f maxExtents{radius, radius, radius};
//                    ende::math::Vec3f minExtents = maxExtents * -1;
//
//                    ende::math::Vec3f lightDir = light.transform().rot().front().unit();
//
//                }
//
//            }
//        }
//
//
//    });


    auto& cullPass = _graph.addPass("cull");
    cullPass.addBufferInput("transforms", transformsResource);
    cullPass.addBufferInput("meshData", meshDataResource);
    cullPass.addBufferInput("materialCounts", materialCountResource);
    cullPass.addBufferOutput("drawCommands", drawCommandsResource);
    cullPass.setDebugColour({0.3, 0.3, 1, 1});

    cullPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto transforms = graph.getResource<BufferResource>("transforms");
        auto meshData = graph.getResource<BufferResource>("meshData");
        auto materialCounts = graph.getResource<BufferResource>("materialCounts");
        auto drawCommands = graph.getResource<BufferResource>("drawCommands");
        cmd.clearDescriptors();
        cmd.bindProgram(*_engine->_cullProgram);
        cmd.bindBindings(nullptr);
        cmd.bindAttributes(nullptr);
        cmd.pushConstants({ &_cullingFrustum, sizeof(_cullingFrustum) });
        cmd.bindBuffer(2, 0, *transforms->handle, true);
        cmd.bindBuffer(2, 1, *meshData->handle, true);
        cmd.bindBuffer(2, 2, *drawCommands->handle, true);
        cmd.bindBuffer(2, 3, *_drawCountBuffer[_engine->device().frameIndex()], true);
        cmd.bindBuffer(2, 4, *materialCounts->handle, true);
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
            cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindProgram(*_engine->_directShadowProgram);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindBuffer(4, 0, *scene._modelBuffer[_engine->device().frameIndex()], true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
            cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
//            cmd.drawIndirectCount(*drawCommands->handle, 0, *_drawCountBuffer[_engine->device().frameIndex()], 0, scene._renderables.size());
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                cmd.drawIndirectCount(*drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), *scene._materialCountBuffer[_engine->device().frameIndex()], material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.forward && !_renderSettings.debugNormals) {
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
        forwardPass.addBufferInput("lightIndices");
        forwardPass.addBufferInput("lightGrid");
        forwardPass.setDebugColour({0.4, 0.1, 0.9, 1});

        forwardPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto lightGrid = graph.getResource<BufferResource>("lightGrid");
            auto lightIndices = graph.getResource<BufferResource>("lightIndices");
            cmd.clearDescriptors();
            if (scene._renderables.empty())
                return;

            cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()]);
//            cmd.bindBuffer(1, 1, *_globalDataBuffer);

            if (!scene._lightData.empty()) {
                cmd.bindBuffer(3, 0, *scene._lightBuffer[_engine->device().frameIndex()], true);
            }

            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            for (u32 i = 0; i < scene._materialCounts.size(); i++) {
                Material* material = &_engine->_materials[i];
                if (!material)
                    continue;
                cmd.bindProgram(*material->getProgram());
                cmd.bindRasterState(material->getRasterState());
                cmd.bindDepthState(material->getDepthState());
                cmd.bindBuffer(2, 0, *material->buffer(), true);
                cmd.bindBuffer(2, 1, *meshData->handle, true);
                cmd.bindBuffer(2, 2, *lightGrid->handle, true);
                cmd.bindBuffer(2, 3, *lightIndices->handle, true);
                cmd.bindBuffer(4, 0, *scene._modelBuffer[_engine->device().frameIndex()], true);

                struct ForwardPush {
                    ende::math::Vec<4, u32> tileSizes;
                    ende::math::Vec<2, u32> screenSize;
                    i32 irradianceIndex;
                    i32 prefilterIndex;
                    i32 brdfIndex;
                } push;
                push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
                push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };

                if (_renderSettings.ibl) {
                    push.irradianceIndex = scene._skyLightIrradiance.index();
                    push.prefilterIndex = scene._skyLightPrefilter.index();
                    push.brdfIndex = _engine->_brdfImage.index();
                } else {
                    push.irradianceIndex = -1;
                    push.prefilterIndex = -1;
                    push.brdfIndex = -1;
                }

                cmd.pushConstants({ &push, sizeof(push) });

                cmd.bindPipeline();
                cmd.bindDescriptors();

                cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer->buffer());
                cmd.bindIndexBuffer(*_engine->_globalIndexBuffer);
                cmd.drawIndirectCount(*drawCommands->handle, scene._materialCounts[i].offset * sizeof(VkDrawIndexedIndirectCommand), *scene._materialCountBuffer[_engine->device().frameIndex()], i * (sizeof(u32) * 2), scene._materialCounts[i].count);
            }
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
            cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()]);
            cmd.bindBuffer(1, 1, *_globalDataBuffer);
            cmd.bindImage(2, 0, _engine->device().getImageView(hdrImage->handle), _engine->device().defaultSampler(), true);
            cmd.bindImage(2, 1, _engine->device().getImageView(backbuffer->handle), _engine->device().defaultSampler(), true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.dispatchCompute(std::ceil(backbuffer->width / 32.f), std::ceil(backbuffer->height / 32.f), 1);
        });
    }

    if (_renderSettings.skybox && scene._skyLightMap) {
        auto& skyboxPass = _graph.addPass("skybox");
        if (scene._hdrSkyLight && _renderSettings.tonemap && !_renderSettings.debugNormals)
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
            cmd.bindBuffer(1, 0, *_cameraBuffer[_engine->device().frameIndex()]);
            cmd.bindImage(2, 0, scene._skyLightMapView, _engine->device().defaultSampler());
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

    if (!_graph.compile(_swapchain))
        throw "cyclical graph found";

    cmd.startPipelineStatistics();
    _graph.execute(cmd, _swapchainFrame.index);
    cmd.stopPipelineStatistics();

}