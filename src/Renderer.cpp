#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/Probe.h>
#include <Cala/Material.h>
#include <Cala/ImGuiContext.h>
#include <Ende/profile/profile.h>

#include "renderPasses/debugPasses.h"
#include "Cala/backend/vulkan/primitives.h"

cala::Renderer::Renderer(cala::Engine* engine, cala::Renderer::Settings settings)
    : _engine(engine),
    _swapchain(nullptr),
    _cameraBuffer{engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING, true),
                  engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING, true)},
    _globalDataBuffer{engine->device().createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING, true),
                      engine->device().createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING, true)},
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
        auto targetBarrier = _shadowTarget->barrier(backend::PipelineStage::TOP, backend::PipelineStage::EARLY_FRAGMENT, backend::Access::NONE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);

        cmd.pipelineBarrier({ &targetBarrier, 1 });
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
    if (!_frameInfo.cmd)
        return false;

    auto result = _swapchain->nextImage();
    if (!result) {
        _engine->logger().error("Device Lost - Frame: {}", _frameInfo.frame);
        _engine->device().printMarkers();
        throw std::runtime_error("Device Lost");
    }
    _swapchainFrame = result.value();
    _frameInfo.cmd->begin();
    _globalData.time = _engine->getRunningTime().milliseconds();
    return true;
}

f64 cala::Renderer::endFrame() {
    _frameInfo.cmd->end();
    u64 waitValue = _engine->device().getFrameValue(_engine->device().prevFrameIndex());
    u64 signalValue = _engine->device().getNextTimelineValue();
    if (!_frameInfo.cmd->submit(_engine->device().getTimelineSemaphore(), waitValue, signalValue, _swapchainFrame.imageAquired)) {
        _engine->logger().error("Error submitting command buffer");
        _engine->device().printMarkers();
        throw std::runtime_error("Error submitting command buffer");
    }
    _engine->device().setFrameValue(_engine->device().frameIndex(), signalValue);
//    if (!_frameInfo.cmd->submit({ &_swapchainFrame.imageAquired, 1 }, _frameInfo.fence)) {
//        _engine->logger().error("Error submitting command buffer");
//        _engine->device().printMarkers();
//        throw std::runtime_error("Error submitting command buffer");
//    }

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

    _globalData.maxDrawCount = scene._renderables.size();
    _globalData.tranformsBufferIndex = scene._modelBuffer[_engine->device().frameIndex()].index();
    _globalData.meshBufferIndex = scene._meshDataBuffer[_engine->device().frameIndex()].index();
    _globalData.lightBufferIndex = scene._lightBuffer[_engine->device().frameIndex()].index();
    _globalData.cameraBufferIndex = _cameraBuffer[_engine->device().frameIndex()].index();

    if (_renderSettings.ibl) {
        _globalData.irradianceIndex = scene._skyLightIrradiance.index();
        _globalData.prefilterIndex = scene._skyLightPrefilter.index();
        _globalData.brdfIndex = _engine->_brdfImage.index();
    } else {
        _globalData.irradianceIndex = -1;
        _globalData.prefilterIndex = -1;
        _globalData.brdfIndex = -1;
    }

    _globalData.nearestRepeatSampler = _engine->device().getSampler({
        .filter = VK_FILTER_NEAREST,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT
    }).index();

    _globalData.linearRepeatSampler = _engine->device().getSampler({
        .filter = VK_FILTER_LINEAR,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT
    }).index();

    _globalData.lodSampler = _engine->device().getSampler({
        .maxLod = 10
    }).index();

    _globalData.shadowSampler = _engine->device().getSampler({
        .filter = VK_FILTER_NEAREST,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    }).index();

    _globalDataBuffer[_engine->device().frameIndex()]->data({ &_globalData, sizeof(_globalData) });

    bool debugViewEnabled = _renderSettings.debugNormals || _renderSettings.debugRoughness || _renderSettings.debugMetallic || _renderSettings.debugWorldPos || _renderSettings.debugUnlit || _renderSettings.debugWireframe || _renderSettings.debugNormalLines || _renderSettings.debugVxgi;
    debugViewEnabled = false;

    backend::vulkan::CommandBuffer& cmd = *_frameInfo.cmd;

    cmd.clearDescriptors();

    _graph.reset();

    {
        // Register resources used by graph
        ImageResource colourAttachment;
        colourAttachment.format = backend::Format::RGBA32_SFLOAT;
        _graph.addResource("hdr", colourAttachment, true);

        ImageResource backbufferAttachment;
        backbufferAttachment.format = backend::Format::RGBA8_UNORM;
        backbufferAttachment.clear = false;
        backbufferAttachment.transient = false;
        _graph.addResource("backbuffer", backbufferAttachment, true);

        ImageResource depthAttachment;
        depthAttachment.format = backend::Format::D32_SFLOAT;
        _graph.addResource("depth", depthAttachment, true);

        BufferResource clustersResource;
        clustersResource.size = sizeof(ende::math::Vec4f) * 2 * 16 * 9 * 24;
        clustersResource.transient = false;
        _graph.addResource("clusters", clustersResource, true);

        BufferResource drawCommandsResource;
        drawCommandsResource.size = scene._renderables.size() * sizeof(VkDrawIndexedIndirectCommand);
        drawCommandsResource.transient = false;
        drawCommandsResource.usage = backend::BufferUsage::INDIRECT;
        _graph.addResource("drawCommands", drawCommandsResource, true);

        _graph.addResource("shadowDrawCommands", drawCommandsResource, true);
        _graph.addResource("vxgiDrawCommands", drawCommandsResource, true);

        BufferResource drawCountResource;
        drawCountResource.size = sizeof(u32);
        drawCountResource.transient = false;
        drawCountResource.usage = backend::BufferUsage::INDIRECT | backend::BufferUsage::STORAGE;
        _graph.addResource("drawCount", drawCountResource, true);
    }



//    ImageResource directDepth;
//    pointDepth.format = backend::Format::D32_SFLOAT;
//    pointDepth.matchSwapchain = false;
//    pointDepth.transient = false;
//    pointDepth.width = 10;
//    pointDepth.height = 10;

    BufferResource globalResource;
    globalResource.handle = _globalDataBuffer[_engine->device().frameIndex()];
    globalResource.size = _globalDataBuffer[_engine->device().frameIndex()]->size();
    globalResource.usage = _globalDataBuffer[_engine->device().frameIndex()]->usage();
    _graph.addResource("global", globalResource, false);

    BufferResource transformsResource;
    transformsResource.handle = scene._modelBuffer[_engine->device().frameIndex()];
    transformsResource.size = scene._modelBuffer[_engine->device().frameIndex()]->size();
    transformsResource.usage = scene._modelBuffer[_engine->device().frameIndex()]->usage();
    _graph.addResource("transforms", transformsResource, false);

    BufferResource meshDataResource;
    meshDataResource.handle = scene._meshDataBuffer[_engine->device().frameIndex()];
    meshDataResource.size = scene._meshDataBuffer[_engine->device().frameIndex()]->size();
    meshDataResource.usage = scene._meshDataBuffer[_engine->device().frameIndex()]->usage();
    _graph.addResource("meshData", meshDataResource, false);

    BufferResource materialCountResource;
    materialCountResource.handle = scene._materialCountBuffer[_engine->device().frameIndex()];
//    materialCountResource.transient = false;
    materialCountResource.size = scene._materialCountBuffer[_engine->device().frameIndex()]->size();
    materialCountResource.usage = scene._materialCountBuffer[_engine->device().frameIndex()]->usage();
    _graph.addResource("materialCounts", materialCountResource, false);

    BufferResource vertexBufferResource;
    vertexBufferResource.handle = _engine->_globalVertexBuffer;
    vertexBufferResource.size = _engine->_globalVertexBuffer->size();
    vertexBufferResource.usage = _engine->_globalVertexBuffer->usage();
    _graph.addResource("vertexBuffer", vertexBufferResource, false);

    BufferResource indexBufferResource;
    indexBufferResource.handle = _engine->_globalIndexBuffer;
    indexBufferResource.size = _engine->_globalIndexBuffer->size();
    indexBufferResource.usage = _engine->_globalIndexBuffer->usage();
    _graph.addResource("indexBuffer", indexBufferResource, false);

    BufferResource cameraResource;
    cameraResource.handle = _cameraBuffer[_engine->device().frameIndex()];
    cameraResource.size = _cameraBuffer[_engine->device().frameIndex()]->size();
    cameraResource.usage = _cameraBuffer[_engine->device().frameIndex()]->usage();
    _graph.addResource("camera", cameraResource, false);

    BufferResource lightsResource;
    lightsResource.handle = scene._lightBuffer[_engine->device().frameIndex()];
    lightsResource.size = scene._lightBuffer[_engine->device().frameIndex()]->size();
    lightsResource.usage = scene._lightBuffer[_engine->device().frameIndex()]->usage();
    _graph.addResource("lights", lightsResource, false);


    _graph.setBackbuffer("backbuffer");

    if (camera.isDirty()) {
        auto& createClusters = _graph.addPass("create_clusters");

        createClusters.addStorageBufferWrite("clusters", backend::PipelineStage::COMPUTE_SHADER);
        createClusters.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

        createClusters.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto clusters = graph.getResource<BufferResource>("clusters");
            cmd.clearDescriptors();
            cmd.bindProgram(_engine->_createClustersProgram);
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

            cmd.pushConstants(backend::ShaderStage::COMPUTE, { &push, sizeof(push) });
            cmd.bindBuffer(1, 0, clusters->handle, true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.dispatchCompute(16, 9, 24);
        });
        camera.setDirty(false);
    }

    {
        BufferResource lightGridResource;
        lightGridResource.size = sizeof(u32) * 2 * 16 * 9 * 24;
        lightGridResource.transient = false;
        _graph.addResource("lightGrid", lightGridResource, true);

        BufferResource lightIndicesResource;
        lightIndicesResource.size = sizeof(u32) * 16 * 9 * 24 * 100;
        lightIndicesResource.transient = false;
        _graph.addResource("lightIndices", lightIndicesResource, true);

        BufferResource lightGlobalIndexResource;
        lightGlobalIndexResource.size = sizeof(u32);
        lightGlobalIndexResource.transient = false;
        _graph.addResource("lightGlobalResource", lightGlobalIndexResource, true);
    }

    auto& cullLights = _graph.addPass("cull_lights");

    cullLights.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferRead("clusters", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferWrite("lightGrid", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferWrite("lightIndices", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferRead("lights", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferWrite("lightGlobalResource", backend::PipelineStage::COMPUTE_SHADER, true);
    cullLights.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

    cullLights.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto global = graph.getResource<BufferResource>("global");
        auto clusters = graph.getResource<BufferResource>("clusters");
        auto lightGrid = graph.getResource<BufferResource>("lightGrid");
        auto lightIndices = graph.getResource<BufferResource>("lightIndices");
        auto lightGlobalIndex = graph.getResource<BufferResource>("lightGlobalResource");
        cmd.clearDescriptors();
        cmd.bindProgram(_engine->_cullLightsProgram);
        cmd.bindBindings(nullptr);
        cmd.bindAttributes(nullptr);
        struct ClusterPush {
            ende::math::Mat4f inverseProjection;
            ende::math::Vec<4, u32> tileSizes;
            ende::math::Vec<2, u32> screenSize;
            f32 near;
            f32 far;
            i32 lightGridIndex;
            i32 lightIndicesIndex;
        } push;
        push.inverseProjection = camera.projection().inverse();
        push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
        push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
        push.near = camera.near();
        push.far = camera.far();
        push.lightGridIndex = lightGrid->handle.index();
        push.lightIndicesIndex = lightIndices->handle.index();

        cmd.pushConstants(backend::ShaderStage::COMPUTE, { &push, sizeof(push) });
        cmd.bindBuffer(1, 0, global->handle);
        cmd.bindBuffer(1, 1, clusters->handle, true);
        cmd.bindBuffer(1, 2, lightGlobalIndex->handle, true);
        cmd.bindBuffer(1, 3, scene._lightCountBuffer[_engine->device().frameIndex()], true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(1, 1, 6);
    });

    if (_renderSettings.debugClusters) {
        auto& debugClusters = _graph.addPass("debug_clusters");

        debugClusters.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
        debugClusters.addColourAttachment("hdr");
        debugClusters.addStorageBufferRead("lightGrid", backend::PipelineStage::FRAGMENT_SHADER);
        debugClusters.addSampledImageRead("depth", backend::PipelineStage::FRAGMENT_SHADER);

        debugClusters.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto global = graph.getResource<BufferResource>("global");
            auto lightGrid = graph.getResource<BufferResource>("lightGrid");
            auto depthBuffer = graph.getResource<ImageResource>("depth");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, global->handle);
            cmd.bindBindings(nullptr);
            cmd.bindAttributes(nullptr);
            cmd.bindBlendState({ true });
            cmd.bindProgram(_engine->_clusterDebugProgram);
            struct ClusterPush {
                ende::math::Vec<4, u32> tileSizes;
                ende::math::Vec<2, u32> screenSize;
            } push;
            push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
            push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
            cmd.pushConstants(backend::ShaderStage::FRAGMENT, { &push, sizeof(push) });
            cmd.bindBuffer(1, 1, lightGrid->handle, true);
            cmd.bindImage(1, 2, _engine->device().getImageView(depthBuffer->handle), _engine->device().defaultShadowSampler());
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.draw(3, 1, 0, 0, false);
            cmd.bindBlendState({ false });
        });
    }

    if (_renderSettings.debugNormals) {
        debugNormalPass(_graph, *_engine, scene);
    }

    if (_renderSettings.debugRoughness) {
        debugRoughnessPass(_graph, *_engine, scene);
    }

    if (_renderSettings.debugMetallic) {
        debugMetallicPass(_graph, *_engine, scene);
    }

    if (_renderSettings.debugUnlit) {
        debugUnlitPass(_graph, *_engine, scene);
    }

    if (_renderSettings.debugWorldPos) {
        debugWorldPositionPass(_graph, *_engine, scene);
    }

    if (_renderSettings.debugWireframe) {
        bool overlay = _renderSettings.tonemap;
        //TODO: overlay works however having both wireframe and normal lines with overlay crashes
//        overlay = false;
        debugWireframePass(_graph, *_engine, scene, _renderSettings, overlay ? "hdr" : "backbuffer");
    }

    if (_renderSettings.debugNormalLines) {
        bool overlay = _renderSettings.tonemap;
//        overlay = false;
        debugNormalLinesPass(_graph, *_engine, scene, _renderSettings, overlay ? "hdr" : "backbuffer");
    }


    {
        ImageResource pointDepth;
        pointDepth.format = backend::Format::D32_SFLOAT;
        pointDepth.matchSwapchain = false;
        pointDepth.transient = false;
        pointDepth.width = 10;
        pointDepth.height = 10;
        _graph.addResource("pointDepth", pointDepth, true);
    }

    auto& pointShadows = _graph.addPass("point_shadows", true);

    pointShadows.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addSampledImageWrite("pointDepth", backend::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
//    pointShadows.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER);
    pointShadows.addVertexBufferRead("vertexBuffer");
    pointShadows.addIndexBufferRead("indexBuffer");
    pointShadows.addIndirectBufferRead("shadowDrawCommands");

    pointShadows.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto global = graph.getResource<BufferResource>("global");
        auto drawCommands = graph.getResource<BufferResource>("shadowDrawCommands");
        auto drawCount = graph.getResource<BufferResource>("drawCount");
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
                    cmd.bindProgram(_engine->_pointShadowCullProgram);
                    cmd.bindBindings(nullptr);
                    cmd.bindAttributes(nullptr);
                    cmd.pushConstants(backend::ShaderStage::COMPUTE, { &shadowFrustum, sizeof(shadowFrustum) });
                    cmd.bindBuffer(1, 0, global->handle);
                    cmd.bindBuffer(2, 0, drawCommands->handle, true);
                    cmd.bindBuffer(2, 1, drawCount->handle, true);
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

                    cmd.bindProgram(_engine->_pointShadowProgram);

                    cmd.bindBuffer(3, 0, scene._lightBuffer[_engine->device().frameIndex()], sizeof(Light::Data) * i, sizeof(Light::Data), true);

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
                    cmd.pushConstants(backend::ShaderStage::VERTEX | backend::ShaderStage::FRAGMENT, { &shadowData, sizeof(shadowData) });

                    auto& renderable = scene._renderables[0].second.first;
                    cmd.bindBindings(renderable.bindings);
                    cmd.bindAttributes(renderable.attributes);

                    cmd.bindBuffer(1, 0, global->handle);

                    cmd.bindPipeline();
                    cmd.bindDescriptors();
                    cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
                    cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
                    cmd.drawIndirectCount(drawCommands->handle, 0, drawCount->handle, 0);

                    cmd.end(*_shadowFramebuffer);

                    auto srcBarrier = _shadowTarget->barrier(backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::TRANSFER, backend::Access::DEPTH_STENCIL_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::TRANSFER_SRC);
                    auto dstBarrier = shadowMap->barrier(backend::PipelineStage::FRAGMENT_SHADER, backend::PipelineStage::TRANSFER, backend::Access::SHADER_READ, backend::Access::TRANSFER_WRITE, backend::ImageLayout::TRANSFER_DST, face);
                    cmd.pipelineBarrier({ &srcBarrier, 1 });
                    cmd.pipelineBarrier({ &dstBarrier, 1 });

                    _shadowTarget->copy(cmd, *shadowMap, 0, face);
                    srcBarrier = _shadowTarget->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::EARLY_FRAGMENT, backend::Access::TRANSFER_READ, backend::Access::DEPTH_STENCIL_WRITE, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                    dstBarrier = shadowMap->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::FRAGMENT_SHADER, backend::Access::TRANSFER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY, face);
                    cmd.pipelineBarrier({ &srcBarrier, 1 });
                    cmd.pipelineBarrier({ &dstBarrier, 1 });
                }
            }
        }
    });

    {
        if (_renderSettings.vxgi) {
//            auto& cullPass = _graph.addPass("vxgiCull");
//
//            cullPass.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
//            cullPass.addStorageBufferRead("transforms", backend::PipelineStage::COMPUTE_SHADER);
//            cullPass.addStorageBufferRead("meshData", backend::PipelineStage::COMPUTE_SHADER);
//            cullPass.addStorageBufferWrite("materialCounts", backend::PipelineStage::COMPUTE_SHADER);
//            cullPass.addStorageBufferWrite("vxgiDrawCommands", backend::PipelineStage::COMPUTE_SHADER);
//            cullPass.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);
//
//            cullPass.setDebugColour({0.3, 0.3, 1, 1});
//
//            cullPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
//                auto global = graph.getResource<BufferResource>("global");
//                auto materialCounts = graph.getResource<BufferResource>("materialCounts");
//                auto drawCommands = graph.getResource<BufferResource>("vxgiDrawCommands");
//
//                ende::math::Mat4f perspective = ende::math::orthographic<f32>(_renderSettings.voxelBounds.first.x(), _renderSettings.voxelBounds.second.x(), _renderSettings.voxelBounds.first.y(), _renderSettings.voxelBounds.second.y(), _renderSettings.voxelBounds.first.z(), _renderSettings.voxelBounds.second.z());
//                ende::math::Frustum frustum(perspective);
//
//                cmd.clearDescriptors();
//                cmd.bindProgram(_engine->_cullProgram);
//                cmd.bindBindings(nullptr);
//                cmd.bindAttributes(nullptr);
//                cmd.pushConstants(backend::ShaderStage::COMPUTE, { &frustum, sizeof(frustum) });
//                cmd.bindBuffer(1, 0, global->handle);
//                cmd.bindBuffer(2, 0, drawCommands->handle, true);
//                cmd.bindBuffer(2, 1, _drawCountBuffer[_engine->device().frameIndex()], true);
//                cmd.bindBuffer(2, 2, materialCounts->handle, true);
//                cmd.bindPipeline();
//                cmd.bindDescriptors();
//                cmd.dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);
//            });

            ImageResource voxelGridResource;
            voxelGridResource.format = backend::Format::RGBA32_SFLOAT;
            voxelGridResource.width = 100;
            voxelGridResource.height = 100;
            voxelGridResource.depth = 100;
            voxelGridResource.matchSwapchain = false;
            _graph.addResource("voxelGrid", voxelGridResource);

            ImageResource voxelOutput;
            voxelOutput.format = backend::Format::RGBA32_SFLOAT;
            voxelOutput.matchSwapchain = false;
            voxelOutput.width = voxelGridResource.width;
            voxelOutput.height = voxelGridResource.height;
            _graph.addResource("voxelOutput", voxelOutput);


            auto& voxelGIPass = _graph.addPass("voxelGI");

//        voxelGIPass.addStorageImageRead("voxelGrid", backend::PipelineStage::FRAGMENT_SHADER);
            voxelGIPass.addStorageImageWrite("voxelGrid", backend::PipelineStage::FRAGMENT_SHADER, true);
            voxelGIPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
            voxelGIPass.addIndirectBufferRead("drawCommands");
            voxelGIPass.addVertexBufferRead("vertexBuffer");
            voxelGIPass.addIndexBufferRead("indexBuffer");

            voxelGIPass.addStorageBufferRead("meshData", backend::PipelineStage::FRAGMENT_SHADER);
            voxelGIPass.addStorageBufferRead("lightIndices", backend::PipelineStage::FRAGMENT_SHADER);
            voxelGIPass.addStorageBufferRead("lightGrid", backend::PipelineStage::FRAGMENT_SHADER);
            voxelGIPass.addStorageBufferRead("lights", backend::PipelineStage::FRAGMENT_SHADER);
            voxelGIPass.addIndirectBufferRead("materialCounts");
            voxelGIPass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

            voxelGIPass.addColourAttachment("voxelOutput");

            voxelGIPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
                auto global = graph.getResource<BufferResource>("global");
                auto drawCommands = graph.getResource<BufferResource>("drawCommands");
                auto lightGrid = graph.getResource<BufferResource>("lightGrid");
                auto lightIndices = graph.getResource<BufferResource>("lightIndices");
                auto materialCounts = graph.getResource<BufferResource>("materialCounts");
                auto voxelGrid = graph.getResource<ImageResource>("voxelGrid");
                cmd.clearDescriptors();
                if (scene._renderables.empty())
                    return;

                cmd.bindBuffer(1, 0, global->handle);

                auto& renderable = scene._renderables[0].second.first;

                cmd.bindBindings(renderable.bindings);
                cmd.bindAttributes(renderable.attributes);

                for (u32 i = 0; i < scene._materialCounts.size(); i++) {
                    Material* material = &_engine->_materials[i];
                    if (!material)
                        continue;
                    cmd.bindProgram(material->getVariant(Material::Variant::VOXELGI));
                    cmd.bindRasterState(material->getRasterState());
                    cmd.bindDepthState({ false });
                    cmd.bindBuffer(2, 0, material->buffer(), true);

                    struct VoxelizePush {
                        ende::math::Mat4f orthographic;
                        ende::math::Vec<4, u32> tileSizes;
                        ende::math::Vec<2, u32> screenSize;
                        i32 lightGridIndex;
                        i32 lightIndicesIndex;
                        i32 voxelGridIndex;
                    } push;
                    push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
                    push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
                    push.lightGridIndex = lightGrid->handle.index();
                    push.lightIndicesIndex = lightIndices->handle.index();
                    push.voxelGridIndex = voxelGrid->handle.index();

                    push.orthographic = ende::math::orthographic<f32>(_renderSettings.voxelBounds.first.x(), _renderSettings.voxelBounds.second.x(), _renderSettings.voxelBounds.first.y(), _renderSettings.voxelBounds.second.y(), _renderSettings.voxelBounds.first.z(), _renderSettings.voxelBounds.second.z());

                    cmd.pushConstants(backend::ShaderStage::VERTEX | backend::ShaderStage::FRAGMENT, { &push, sizeof(push) });

                    cmd.bindPipeline();
                    cmd.bindDescriptors();

                    cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
                    cmd.bindIndexBuffer(_engine->_globalIndexBuffer);

                    u32 drawCommandOffset = scene._materialCounts[i].offset * sizeof(VkDrawIndexedIndirectCommand);
                    u32 countOffset = i * (sizeof(u32) * 2);
                    cmd.drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
                }
            });
        }
        if (_renderSettings.debugVxgi) {
            ImageResource colourAttachment1;
            colourAttachment1.format = backend::Format::RGBA32_SFLOAT;
            _graph.addResource("voxelVisualised", colourAttachment1);


            auto& voxelVisualisePass = _graph.addPass("voxelVisualisation", true);

//        voxelVisualisePass.addColourAttachment("voxelVisualised");
//        voxelVisualisePass.addColourAttachment("backbuffer");

//        voxelVisualisePass.addStorageImageWrite("voxelVisualised", backend::PipelineStage::COMPUTE_SHADER);
            voxelVisualisePass.addStorageImageWrite("backbuffer", backend::PipelineStage::COMPUTE_SHADER);
            voxelVisualisePass.addStorageImageRead("voxelGrid", backend::PipelineStage::COMPUTE_SHADER);
            voxelVisualisePass.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
            voxelVisualisePass.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

            voxelVisualisePass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
                auto global = graph.getResource<BufferResource>("global");
                auto voxelGrid = graph.getResource<ImageResource>("voxelGrid");
                auto voxelVisualised = graph.getResource<ImageResource>("backbuffer");
//            auto voxelVisualised = graph.getResource<ImageResource>("voxelVisualised");
                cmd.clearDescriptors();
                cmd.bindProgram(_engine->getProgram(Engine::ProgramType::VOXEL_VISUALISE));
                cmd.bindBindings(nullptr);
                cmd.bindAttributes(nullptr);
                cmd.bindBuffer(1, 0, global->handle);
                cmd.bindImage(2, 0, _engine->device().getImageView(voxelVisualised->handle), _engine->device().defaultSampler(), true);
                struct VoxelPush {
                    ende::math::Mat4f voxelOrthographic;
                    i32 voxelGridIndex;
                } push;
                push.voxelGridIndex = voxelGrid->handle.index();
                push.voxelOrthographic = ende::math::orthographic<f32>(_renderSettings.voxelBounds.first.x(), _renderSettings.voxelBounds.second.x(), _renderSettings.voxelBounds.first.y(), _renderSettings.voxelBounds.second.y(), _renderSettings.voxelBounds.first.z(), _renderSettings.voxelBounds.second.z());
                cmd.pushConstants(backend::ShaderStage::COMPUTE, { &push, sizeof(push) });
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.dispatchCompute(std::ceil(voxelVisualised->width / 32.f), std::ceil(voxelVisualised->height / 32.f), 1);
            });
        }
    }


    auto& cullPass = _graph.addPass("cull");

    cullPass.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferRead("transforms", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferRead("meshData", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferWrite("materialCounts", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferWrite("drawCommands", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

    cullPass.setDebugColour({0.3, 0.3, 1, 1});

    cullPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto global = graph.getResource<BufferResource>("global");
        auto materialCounts = graph.getResource<BufferResource>("materialCounts");
        auto drawCommands = graph.getResource<BufferResource>("drawCommands");
        cmd.clearDescriptors();
        cmd.bindProgram(_engine->_cullProgram);
        cmd.bindBindings(nullptr);
        cmd.bindAttributes(nullptr);
        cmd.pushConstants(backend::ShaderStage::COMPUTE, { &_cullingFrustum, sizeof(_cullingFrustum) });
        cmd.bindBuffer(1, 0, global->handle);
        cmd.bindBuffer(2, 0, drawCommands->handle, true);
        cmd.bindBuffer(2, 1, materialCounts->handle, true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);
    });


    if (_renderSettings.depthPre) {
        auto& depthPrePass = _graph.addPass("depth_pre");

        depthPrePass.addDepthAttachment("depth");

        depthPrePass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
        depthPrePass.addIndirectBufferRead("drawCommands");
        depthPrePass.addIndirectBufferRead("materialCounts");
        depthPrePass.addVertexBufferRead("vertexBuffer");
        depthPrePass.addIndexBufferRead("indexBuffer");
        depthPrePass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

        depthPrePass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto global = graph.getResource<BufferResource>("global");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, global->handle);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindProgram(_engine->_directShadowProgram);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
                u32 countOffset = material * (sizeof(u32) * 2);
                cmd.drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
            }
        });
    }

    if (_renderSettings.forward && !debugViewEnabled) {
        auto& forwardPass = _graph.addPass("forward");
        if (_renderSettings.tonemap)
            forwardPass.addColourAttachment("hdr");
        else
            forwardPass.addColourAttachment("backbuffer");
        if (_renderSettings.depthPre)
            forwardPass.addDepthReadAttachment("depth");
        else
            forwardPass.addDepthAttachment("depth");

        forwardPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addSampledImageRead("pointDepth", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addIndirectBufferRead("drawCommands");
        forwardPass.addStorageBufferRead("lightIndices", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addStorageBufferRead("lightGrid", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addStorageBufferRead("lights", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addStorageBufferRead("meshData", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addIndirectBufferRead("materialCounts");
        forwardPass.addVertexBufferRead("vertexBuffer");
        forwardPass.addIndexBufferRead("indexBuffer");
        forwardPass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

//        forwardPass.addStorageImageRead("voxelGrid", backend::PipelineStage::FRAGMENT_SHADER);
//        forwardPass.addStorageImageRead("voxelVisualised", backend::PipelineStage::FRAGMENT_SHADER);

        forwardPass.setDebugColour({0.4, 0.1, 0.9, 1});

        forwardPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto global = graph.getResource<BufferResource>("global");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto lightGrid = graph.getResource<BufferResource>("lightGrid");
            auto lightIndices = graph.getResource<BufferResource>("lightIndices");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            if (scene._renderables.empty())
                return;

            cmd.bindBuffer(1, 0, global->handle);

            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);

            for (u32 i = 0; i < scene._materialCounts.size(); i++) {
                Material* material = &_engine->_materials[i];
                if (!material)
                    continue;
                cmd.bindProgram(material->getVariant(Material::Variant::LIT));
                cmd.bindRasterState(material->getRasterState());
                cmd.bindDepthState(material->getDepthState());
                cmd.bindBuffer(2, 0, material->buffer(), true);

                struct ForwardPush {
                    ende::math::Vec<4, u32> tileSizes;
                    ende::math::Vec<2, u32> screenSize;
                    i32 lightGridIndex;
                    i32 lightIndicesIndex;
                } push;
                push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
                push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
                push.lightGridIndex = lightGrid->handle.index();
                push.lightIndicesIndex = lightIndices->handle.index();

                cmd.pushConstants(backend::ShaderStage::FRAGMENT, { &push, sizeof(push) });

                cmd.bindPipeline();
                cmd.bindDescriptors();

                cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
                cmd.bindIndexBuffer(_engine->_globalIndexBuffer);

                u32 drawCommandOffset = scene._materialCounts[i].offset * sizeof(VkDrawIndexedIndirectCommand);
                u32 countOffset = i * (sizeof(u32) * 2);
                cmd.drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
            }
        });
    }

    if (_renderSettings.tonemap && !debugViewEnabled) {
        auto& tonemapPass = _graph.addPass("tonemap");

        tonemapPass.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
        tonemapPass.addStorageImageWrite("backbuffer", backend::PipelineStage::COMPUTE_SHADER);
        tonemapPass.addStorageImageRead("hdr", backend::PipelineStage::COMPUTE_SHADER);

        tonemapPass.setDebugColour({0.1, 0.4, 0.7, 1});
        tonemapPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto global = graph.getResource<BufferResource>("global");
            auto hdrImage = graph.getResource<ImageResource>("hdr");
            auto backbuffer = graph.getResource<ImageResource>("backbuffer");
            cmd.clearDescriptors();
            cmd.bindProgram(_engine->_tonemapProgram);
            cmd.bindBindings(nullptr);
            cmd.bindAttributes(nullptr);
            cmd.bindBuffer(1, 0, global->handle);
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
            skyboxPass.addColourAttachment("hdr");
        else
            skyboxPass.addColourAttachment("backbuffer");
        skyboxPass.addDepthReadAttachment("depth");
        skyboxPass.setDebugColour({0, 0.7, 0.1, 1});

        skyboxPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

        skyboxPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto global = graph.getResource<BufferResource>("global");
            cmd.clearDescriptors();
            cmd.bindProgram(_engine->_skyboxProgram);
            cmd.bindRasterState({ backend::CullMode::NONE });
            cmd.bindDepthState({ true, false, backend::CompareOp::LESS_EQUAL });
            cmd.bindBindings({ &_engine->_cube->_binding, 1 });
            cmd.bindAttributes(_engine->_cube->_attributes);
            cmd.bindBuffer(1, 0, global->handle);
            i32 skyMapIndex = scene._skyLightMap.index();
            cmd.pushConstants(backend::ShaderStage::FRAGMENT, { &skyMapIndex, sizeof(skyMapIndex) });

            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            cmd.draw(_engine->_cube->indexCount, 1, _engine->_cube->firstIndex, 0);
        });
    }

    if (imGui) {
        auto& uiPass = _graph.addPass("ui");
        uiPass.addColourAttachment("backbuffer");
        uiPass.setDebugColour({0.7, 0.1, 0.4, 1});

        uiPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            imGui->render(cmd);
        });
    }

    if (!_graph.compile(_swapchain))
        throw std::runtime_error("cyclical graph found");

    cmd.startPipelineStatistics();
    _graph.execute(cmd, _swapchainFrame.index);
    cmd.stopPipelineStatistics();

}