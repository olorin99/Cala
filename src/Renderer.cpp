#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/Probe.h>
#include <Cala/Material.h>
#include <Cala/ImGuiContext.h>
#include <Ende/profile/profile.h>
#include <Ende/thread/thread.h>

#include "renderPasses/debugPasses.h"
#include "Cala/backend/vulkan/primitives.h"

cala::Renderer::Renderer(cala::Engine* engine, cala::Renderer::Settings settings)
    : _engine(engine),
    _swapchain(nullptr),
    _graph(engine),
    _renderSettings(settings),
    _cullingFrustum(ende::math::perspective(45.f, 1920.f / 1080.f, 0.1f, 1000.f, true))
{
    _engine->device().setBindlessSetIndex(0);

    for (auto& buffer : _cameraBuffer) {
        buffer = engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING, true);
    }
    for (auto& buffer : _globalDataBuffer) {
        buffer = engine->device().createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING, true);
    }

    _shadowTarget = _engine->device().createImage({
        1024, 1024, 1,
        backend::Format::D32_SFLOAT,
        1, 1,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT | backend::ImageUsage::TRANSFER_SRC
    });
    _engine->device().immediate([&](backend::vulkan::CommandHandle cmd) {
        auto targetBarrier = _shadowTarget->barrier(backend::PipelineStage::TOP, backend::PipelineStage::EARLY_FRAGMENT, backend::Access::NONE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);

        cmd->pipelineBarrier({ &targetBarrier, 1 });
    });
    backend::vulkan::RenderPass::Attachment shadowAttachment = {
            cala::backend::Format::D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            backend::LoadOp::CLEAR,
            backend::StoreOp::STORE,
            backend::LoadOp::DONT_CARE,
            backend::StoreOp::DONT_CARE,
            backend::ImageLayout::UNDEFINED,
            backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT,
            backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT
    };
    auto shadowRenderPass = _engine->device().getRenderPass({&shadowAttachment, 1 });
    u32 h = _shadowTarget.index();
    _shadowFramebuffer = _engine->device().getFramebuffer(shadowRenderPass, {&_engine->device().getImageView(_shadowTarget).view, 1 }, {&h, 1 }, 1024, 1024);
}

bool cala::Renderer::beginFrame(cala::backend::vulkan::Swapchain* swapchain) {
    _swapchain = swapchain;
    assert(_swapchain);
    _frameInfo = _engine->device().beginFrame();

    if (_renderSettings.boundedFrameTime) {
        f64 frameTime = _engine->device().milliseconds();
        f64 frameTimeDiff = frameTime - _renderSettings.millisecondTarget;
        if (frameTimeDiff < 0)
            ende::thread::sleep(ende::time::Duration::fromMilliseconds(-frameTimeDiff));
    }

    if (!_frameInfo.cmd)
        return false;

    auto result = _swapchain->nextImage();
    if (!result) {
        _engine->logger().error("Device Lost - Frame: {}", _frameInfo.frame);
        _engine->device().printMarkers();
        throw std::runtime_error("Device Lost");
    }
    _swapchainFrame = std::move(result.value());
    _frameInfo.cmd->begin();
    _globalData.time = _engine->getRunningTime().milliseconds();
    return true;
}

f64 cala::Renderer::endFrame() {
    _frameInfo.cmd->end();
    if (_engine->device().usingTimeline()) {
        u64 waitValue = _engine->device().getFrameValue(_engine->device().prevFrameIndex());
        u64 signalValue = _engine->device().getTimelineSemaphore().increment();
//        if (!_frameInfo.cmd->submit(_engine->device().getTimelineSemaphore(), waitValue, signalValue, &_swapchainFrame.semaphores.acquire, &_swapchainFrame.semaphores.present)) {
        std::array<backend::vulkan::CommandBuffer::SemaphoreSubmit, 2> wait({ { &_engine->device().getTimelineSemaphore(), waitValue }, { &_swapchainFrame.semaphores.acquire, 0 } });
        std::array<backend::vulkan::CommandBuffer::SemaphoreSubmit, 2> signal({ { &_engine->device().getTimelineSemaphore(), signalValue }, { &_swapchainFrame.semaphores.present, 0 } });
        if (!_frameInfo.cmd->submit(wait, signal)) {
            _engine->logger().error("Error submitting command buffer");
            _engine->device().printMarkers();
            throw std::runtime_error("Error submitting command buffer");
        }
        _engine->device().setFrameValue(_engine->device().frameIndex(), signalValue);
    } else {
        std::array<backend::vulkan::CommandBuffer::SemaphoreSubmit, 1> wait({{ &_swapchainFrame.semaphores.acquire, 0 }});
        std::array<backend::vulkan::CommandBuffer::SemaphoreSubmit, 1> signal({{ &_swapchainFrame.semaphores.present, 0 }});
        if (!_frameInfo.cmd->submit(wait, signal, _frameInfo.fence)) {
            _engine->logger().error("Error submitting command buffer");
            _engine->device().printMarkers();
            throw std::runtime_error("Error submitting command buffer");
        }
    }

    assert(_swapchain);
    _swapchain->present(std::move(_swapchainFrame));
    _engine->device().endFrame();

    _stats.drawCallCount = _frameInfo.cmd->drawCalls();

    return static_cast<f64>(_engine->device().milliseconds()) / 1000.f;
}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera, ImGuiContext* imGui) {
    PROFILE_NAMED("Renderer::render");
    if (!_renderSettings.freezeFrustum)
        _cullingFrustum = camera.frustum();
    auto cameraData = camera.data();
    _cameraBuffer[_engine->device().frameIndex()]->data(std::span<Camera::Data>(&cameraData, 1));

    bool overlayDebug = _renderSettings.debugWireframe || _renderSettings.debugNormalLines || _renderSettings.debugClusters;
    bool fullscreenDebug = _renderSettings.debugNormals || _renderSettings.debugWorldPos || _renderSettings.debugUnlit || _renderSettings.debugMetallic || _renderSettings.debugRoughness || _renderSettings.debugVxgi;
    bool debugViewEnabled = overlayDebug || fullscreenDebug;
    if (_renderSettings.debugVxgi)
        _renderSettings.vxgi = true;

    backend::vulkan::CommandHandle cmd = _frameInfo.cmd;

    cmd->clearDescriptors();

    _graph.reset();

    {
        // Register resources used by graph
        {
            ImageResource colourAttachment;
            colourAttachment.format = backend::Format::RGBA32_SFLOAT;
            _graph.addImageResource("hdr", colourAttachment);
        }
        {
            ImageResource backbufferAttachment;
            backbufferAttachment.format = backend::Format::RGBA8_UNORM;
            _graph.addImageResource("backbuffer", backbufferAttachment);
            _graph.addAlias("backbuffer", "backbuffer-debug");
        }
        {
            ImageResource depthAttachment;
            depthAttachment.format = backend::Format::D32_SFLOAT;
            _graph.addImageResource("depth", depthAttachment);
        }
        {
            BufferResource clustersResource;
            clustersResource.size = sizeof(ende::math::Vec4f) * 2 * 16 * 9 * 24;
            _graph.addBufferResource("clusters", clustersResource);
        }
        {
            BufferResource drawCommandsResource;
            drawCommandsResource.size = scene._renderables.size() * sizeof(VkDrawIndexedIndirectCommand);
            drawCommandsResource.usage = backend::BufferUsage::INDIRECT;
            _graph.addBufferResource("drawCommands", drawCommandsResource);

            _graph.addBufferResource("shadowDrawCommands", drawCommandsResource);
            _graph.addBufferResource("vxgiDrawCommands", drawCommandsResource);
        }
        {
            BufferResource drawCountResource;
            drawCountResource.size = sizeof(u32);
            drawCountResource.usage = backend::BufferUsage::INDIRECT | backend::BufferUsage::STORAGE;
            _graph.addBufferResource("drawCount", drawCountResource);
        }

    }



//    ImageResource directDepth;
//    pointDepth.format = backend::Format::D32_SFLOAT;
//    pointDepth.matchSwapchain = false;
//    pointDepth.transient = false;
//    pointDepth.width = 10;
//    pointDepth.height = 10;
    {
        BufferResource globalResource;
        globalResource.size = _globalDataBuffer[_engine->device().frameIndex()]->size();
        globalResource.usage = _globalDataBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("global", globalResource, _globalDataBuffer[_engine->device().frameIndex()]);
    }
    {
        BufferResource transformsResource;
        transformsResource.size = scene._modelBuffer[_engine->device().frameIndex()]->size();
        transformsResource.usage = scene._modelBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("transforms", transformsResource, scene._modelBuffer[_engine->device().frameIndex()]);
    }
    {
        BufferResource meshDataResource;
        meshDataResource.size = scene._meshDataBuffer[_engine->device().frameIndex()]->size();
        meshDataResource.usage = scene._meshDataBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("meshData", meshDataResource, scene._meshDataBuffer[_engine->device().frameIndex()]);
    }
    {
        BufferResource materialCountResource;
        materialCountResource.size = scene._materialCountBuffer[_engine->device().frameIndex()]->size();
        materialCountResource.usage = scene._materialCountBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("materialCounts", materialCountResource, scene._materialCountBuffer[_engine->device().frameIndex()]);
    }
    {
        BufferResource vertexBufferResource;
        vertexBufferResource.size = _engine->_globalVertexBuffer->size();
        vertexBufferResource.usage = _engine->_globalVertexBuffer->usage();
        _graph.addBufferResource("vertexBuffer", vertexBufferResource, _engine->_globalVertexBuffer);
    }
    {
        BufferResource indexBufferResource;
        indexBufferResource.size = _engine->_globalIndexBuffer->size();
        indexBufferResource.usage = _engine->_globalIndexBuffer->usage();
        _graph.addBufferResource("indexBuffer", indexBufferResource, _engine->_globalIndexBuffer);
    }
    {
        BufferResource cameraResource;
        cameraResource.size = _cameraBuffer[_engine->device().frameIndex()]->size();
        cameraResource.usage = _cameraBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("camera", cameraResource, _cameraBuffer[_engine->device().frameIndex()]);
    }
    {
        BufferResource lightsResource;
        lightsResource.size = scene._lightBuffer[_engine->device().frameIndex()]->size();
        lightsResource.usage = scene._lightBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("lights", lightsResource, scene._lightBuffer[_engine->device().frameIndex()]);
    }



    if (camera.isDirty()) {
        auto& createClusters = _graph.addPass("create_clusters", RenderPass::Type::COMPUTE);

        createClusters.addStorageBufferWrite("clusters", backend::PipelineStage::COMPUTE_SHADER);
        createClusters.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

        createClusters.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto clusters = graph.getBuffer("clusters");
            cmd->clearDescriptors();
            cmd->bindProgram(_engine->_createClustersProgram);
            cmd->bindBindings({});
            cmd->bindAttributes({});
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

            cmd->pushConstants(backend::ShaderStage::COMPUTE, push);
            cmd->bindBuffer(1, 0, clusters, true);
            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->dispatchCompute(16, 9, 24);
        });
        camera.setDirty(false);
    }

    {
        BufferResource lightGridResource;
        lightGridResource.size = sizeof(u32) * 2 * 16 * 9 * 24;
        _graph.addBufferResource("lightGrid", lightGridResource);

        BufferResource lightIndicesResource;
        lightIndicesResource.size = sizeof(u32) * 16 * 9 * 24 * 100;
        _graph.addBufferResource("lightIndices", lightIndicesResource);

        BufferResource lightGlobalIndexResource;
        lightGlobalIndexResource.size = sizeof(u32);
        _graph.addBufferResource("lightGlobalResource", lightGlobalIndexResource);
    }

    auto& cullLights = _graph.addPass("cull_lights", RenderPass::Type::COMPUTE);

    cullLights.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferRead("clusters", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferWrite("lightGrid", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferWrite("lightIndices", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferRead("lights", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferWrite("lightGlobalResource", backend::PipelineStage::COMPUTE_SHADER);
    cullLights.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

    cullLights.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto clusters = graph.getBuffer("clusters");
        auto lightGrid = graph.getBuffer("lightGrid");
        auto lightIndices = graph.getBuffer("lightIndices");
        auto lightGlobalIndex = graph.getBuffer("lightGlobalResource");
        cmd->clearDescriptors();
        cmd->bindProgram(_engine->_cullLightsProgram);
        cmd->bindBindings({});
        cmd->bindAttributes({});
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
        push.lightGridIndex = lightGrid.index();
        push.lightIndicesIndex = lightIndices.index();

        cmd->pushConstants(backend::ShaderStage::COMPUTE, push);
        cmd->bindBuffer(1, 0, global, true);
        cmd->bindBuffer(1, 1, clusters, true);
        cmd->bindBuffer(1, 2, lightGlobalIndex, true);
//        cmd->bindBuffer(1, 3, scene._lightCountBuffer[_engine->device().frameIndex()], true);
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatchCompute(1, 1, 6);
    });

    if (_renderSettings.debugClusters) {
        debugClusters(_graph, *_engine, *_swapchain);
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
        debugWireframePass(_graph, *_engine, scene, _renderSettings);
    }

    if (_renderSettings.debugNormalLines) {
        debugNormalLinesPass(_graph, *_engine, scene, _renderSettings);
    }

    if (_renderSettings.debugVxgi) {
        debugVxgi(_graph, *_engine);
    }


    {
        ImageResource pointDepth;
        pointDepth.format = backend::Format::RGBA8_UNORM;
        pointDepth.matchSwapchain = false;
        pointDepth.width = 10;
        pointDepth.height = 10;
        _graph.addImageResource("pointDepth", pointDepth);
    }

    auto& pointShadows = _graph.addPass("point_shadows", RenderPass::Type::COMPUTE);

    pointShadows.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageImageWrite("pointDepth", backend::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
//    pointShadows.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER);
    pointShadows.addVertexRead("vertexBuffer");
    pointShadows.addIndexRead("indexBuffer");
    pointShadows.addIndirectRead("shadowDrawCommands");

    pointShadows.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("shadowDrawCommands");
        auto drawCount = graph.getBuffer("drawCount");
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

                    cmd->clearDescriptors();
                    cmd->bindProgram(_engine->_pointShadowCullProgram);
                    cmd->bindBindings({});
                    cmd->bindAttributes({});
                    cmd->pushConstants(backend::ShaderStage::COMPUTE, shadowFrustum);
                    cmd->bindBuffer(1, 0, global, true);
                    cmd->bindBuffer(2, 0, drawCommands, true);
                    cmd->bindBuffer(2, 1, drawCount, true);
                    cmd->bindPipeline();
                    cmd->bindDescriptors();
                    cmd->dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);


                    cmd->begin(*_shadowFramebuffer);

                    cmd->clearDescriptors();
                    cmd->bindRasterState({
                        backend::CullMode::FRONT
                    });
                    cmd->bindDepthState({
                        true, true,
                        backend::CompareOp::LESS_EQUAL
                    });

                    cmd->bindProgram(_engine->_pointShadowProgram);

                    cmd->bindBuffer(3, 0, scene._lightBuffer[_engine->device().frameIndex()], sizeof(Light::Data) * i, sizeof(Light::Data), true);

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
                    cmd->pushConstants(backend::ShaderStage::VERTEX | backend::ShaderStage::FRAGMENT, shadowData);

                    auto& renderable = scene._renderables[0].second.first;
                    cmd->bindBindings(renderable.bindings);
                    cmd->bindAttributes(renderable.attributes);

                    cmd->bindBuffer(1, 0, global, true);

                    cmd->bindPipeline();
                    cmd->bindDescriptors();
                    cmd->bindVertexBuffer(0, _engine->_globalVertexBuffer);
                    cmd->bindIndexBuffer(_engine->_globalIndexBuffer);
                    cmd->drawIndirectCount(drawCommands, 0, drawCount, 0);

                    cmd->end(*_shadowFramebuffer);

                    auto srcBarrier = _shadowTarget->barrier(backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::TRANSFER, backend::Access::DEPTH_STENCIL_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::TRANSFER_SRC);
                    auto dstBarrier = shadowMap->barrier(backend::PipelineStage::FRAGMENT_SHADER, backend::PipelineStage::TRANSFER, backend::Access::SHADER_READ, backend::Access::TRANSFER_WRITE, backend::ImageLayout::TRANSFER_DST, face);
                    cmd->pipelineBarrier({ &srcBarrier, 1 });
                    cmd->pipelineBarrier({ &dstBarrier, 1 });

                    _shadowTarget->copy(cmd, *shadowMap, 0, face);
                    srcBarrier = _shadowTarget->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::EARLY_FRAGMENT, backend::Access::TRANSFER_READ, backend::Access::DEPTH_STENCIL_WRITE, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                    dstBarrier = shadowMap->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::FRAGMENT_SHADER, backend::Access::TRANSFER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY, face);
                    cmd->pipelineBarrier({ &srcBarrier, 1 });
                    cmd->pipelineBarrier({ &dstBarrier, 1 });
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
//            cullPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
//                auto global = graph.getBuffer()("global");
//                auto materialCounts = graph.getBuffer()("materialCounts");
//                auto drawCommands = graph.getBuffer()("vxgiDrawCommands");
//
//                ende::math::Mat4f perspective = ende::math::orthographic<f32>(_renderSettings.voxelBounds.first.x(), _renderSettings.voxelBounds.second.x(), _renderSettings.voxelBounds.first.y(), _renderSettings.voxelBounds.second.y(), _renderSettings.voxelBounds.first.z(), _renderSettings.voxelBounds.second.z());
//                ende::math::Frustum frustum(perspective);
//
//                cmd->clearDescriptors();
//                cmd->bindProgram(_engine->_cullProgram);
//                cmd->bindBindings(nullptr);
//                cmd->bindAttributes(nullptr);
//                cmd->pushConstants(backend::ShaderStage::COMPUTE, { &frustum, sizeof(frustum) });
//                cmd->bindBuffer(1, 0, global, true);
//                cmd->bindBuffer(2, 0, drawCommands, true);
//                cmd->bindBuffer(2, 1, _drawCountBuffer[_engine->device().frameIndex()], true);
//                cmd->bindBuffer(2, 2, materialCounts, true);
//                cmd->bindPipeline();
//                cmd->bindDescriptors();
//                cmd->dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);
//            });

            ImageResource voxelGridResource;
            voxelGridResource.format = backend::Format::RGBA32_SFLOAT;
            voxelGridResource.width = _renderSettings.voxelGridDimensions.x();
            voxelGridResource.height = _renderSettings.voxelGridDimensions.y();
            voxelGridResource.depth = _renderSettings.voxelGridDimensions.z();
            voxelGridResource.mipLevels = 5;
            voxelGridResource.matchSwapchain = false;
            _graph.addImageResource("voxelGrid", voxelGridResource);
            _graph.addAlias("voxelGrid", "voxelGridClear");
            _graph.addAlias("voxelGrid", "voxelGridMipMapped");

            ImageResource voxelOutput;
            voxelOutput.format = backend::Format::RGBA32_SFLOAT;
            voxelOutput.matchSwapchain = false;
            voxelOutput.width = voxelGridResource.width;
            voxelOutput.height = voxelGridResource.height;
            _graph.addImageResource("voxelOutput", voxelOutput);


            {
                auto& clearVoxelGridPass = _graph.addPass("clear-voxelGrid", RenderPass::Type::TRANSFER);

                clearVoxelGridPass.addTransferWrite("voxelGridClear");

                clearVoxelGridPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
                    auto voxelGrid = graph.getImage("voxelGrid");
                    cmd->clearImage(voxelGrid);
                });
            }
            {
                auto& voxelMipMapPass = _graph.addPass("voxel-mipmap", RenderPass::Type::TRANSFER);
                voxelMipMapPass.addTransferWrite("voxelGridMipMapped");
                voxelMipMapPass.addTransferRead("voxelGrid");

                voxelMipMapPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
                    auto voxelGrid = graph.getImage("voxelGrid");
                    voxelGrid->generateMips(cmd);
                });
            }
            {
                auto& voxelGIPass = _graph.addPass("voxelGI");

                voxelGIPass.setDimensions(voxelGridResource.width, voxelGridResource.height);

                voxelGIPass.addStorageImageRead("voxelGridClear", backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addStorageImageWrite("voxelGrid", backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addIndirectRead("drawCommands");
                voxelGIPass.addVertexRead("vertexBuffer");
                voxelGIPass.addIndexRead("indexBuffer");

                voxelGIPass.addStorageBufferRead("meshData", backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addStorageBufferRead("lightIndices", backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addStorageBufferRead("lightGrid", backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addStorageBufferRead("lights", backend::PipelineStage::FRAGMENT_SHADER);
                voxelGIPass.addIndirectRead("materialCounts");
                voxelGIPass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

//                voxelGIPass.addColourWrite("voxelOutput");

                voxelGIPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
                    auto global = graph.getBuffer("global");
                    auto drawCommands = graph.getBuffer("drawCommands");
                    auto lightGrid = graph.getBuffer("lightGrid");
                    auto lightIndices = graph.getBuffer("lightIndices");
                    auto materialCounts = graph.getBuffer("materialCounts");
                    auto voxelGrid = graph.getImage("voxelGrid");
                    cmd->clearDescriptors();
                    if (scene._renderables.empty())
                        return;

                    cmd->bindBuffer(1, 0, global, true);

                    auto& renderable = scene._renderables[0].second.first;

                    cmd->bindBindings(renderable.bindings);
                    cmd->bindAttributes(renderable.attributes);

                    for (u32 i = 0; i < scene._materialCounts.size(); i++) {
                        Material* material = &_engine->_materials[i];
                        if (!material)
                            continue;
                        cmd->bindProgram(material->getVariant(Material::Variant::VOXELGI));
                        cmd->bindRasterState({
                            backend::CullMode::NONE
                        });
                        cmd->bindDepthState({ false });
                        cmd->bindBuffer(2, 0, material->buffer(), true);

                        struct VoxelizePush {
                            ende::math::Vec<4, u32> voxelGridSize;
                            ende::math::Vec<4, u32> tileSizes;
                            i32 lightGridIndex;
                            i32 lightIndicesIndex;
                        } push;
                        push.voxelGridSize = { voxelGrid->width(), voxelGrid->height(), voxelGrid->depth(), 0 };
                        push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };;
                        push.lightGridIndex = lightGrid.index();
                        push.lightIndicesIndex = lightIndices.index();

                        cmd->pushConstants(backend::ShaderStage::VERTEX | backend::ShaderStage::GEOMETRY | backend::ShaderStage::FRAGMENT, push);

                        cmd->bindPipeline();
                        cmd->bindDescriptors();

                        cmd->bindVertexBuffer(0, _engine->_globalVertexBuffer);
                        cmd->bindIndexBuffer(_engine->_globalIndexBuffer);

                        u32 drawCommandOffset = scene._materialCounts[i].offset * sizeof(VkDrawIndexedIndirectCommand);
                        u32 countOffset = i * (sizeof(u32) * 2);
                        cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
                    }
                });
            }
        }
    }


    auto& cullPass = _graph.addPass("cull", RenderPass::Type::COMPUTE);

    cullPass.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferRead("transforms", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferRead("meshData", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferWrite("materialCounts", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferWrite("drawCommands", backend::PipelineStage::COMPUTE_SHADER);
    cullPass.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

    cullPass.setDebugColour({0.3, 0.3, 1, 1});

    cullPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto materialCounts = graph.getBuffer("materialCounts");
        auto drawCommands = graph.getBuffer("drawCommands");
        cmd->clearDescriptors();
        cmd->bindProgram(_engine->_cullProgram);
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->pushConstants(backend::ShaderStage::COMPUTE, _cullingFrustum);
        cmd->bindBuffer(1, 0, global, true);
        cmd->bindBuffer(2, 0, drawCommands, true);
        cmd->bindBuffer(2, 1, materialCounts, true);
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);
    });


    if (_renderSettings.depthPre) {
        auto& depthPrePass = _graph.addPass("depth_pre");

        depthPrePass.addDepthWrite("depth");

        depthPrePass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
        depthPrePass.addIndirectRead("drawCommands");
        depthPrePass.addIndirectRead("materialCounts");
        depthPrePass.addVertexRead("vertexBuffer");
        depthPrePass.addIndexRead("indexBuffer");
        depthPrePass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

        depthPrePass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            auto drawCommands = graph.getBuffer("drawCommands");
            auto materialCounts = graph.getBuffer("materialCounts");
            cmd->clearDescriptors();
            cmd->bindBuffer(1, 0, global, true);
            auto& renderable = scene._renderables[0].second.first;

            cmd->bindBindings(renderable.bindings);
            cmd->bindAttributes(renderable.attributes);
            cmd->bindProgram(_engine->_directShadowProgram);
            cmd->bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd->bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
                u32 countOffset = material * (sizeof(u32) * 2);
                cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
            }
        });
    }

    if (_renderSettings.forward && !fullscreenDebug) {
        auto& forwardPass = _graph.addPass("forward");
        if (_renderSettings.tonemap)
            forwardPass.addColourWrite("hdr");
        else
            forwardPass.addColourWrite("backbuffer");
        if (_renderSettings.depthPre)
            forwardPass.addDepthRead("depth");
        else
            forwardPass.addDepthWrite("depth");

        forwardPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addSampledImageRead("pointDepth", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addIndirectRead("drawCommands");
        forwardPass.addStorageBufferRead("lightIndices", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addStorageBufferRead("lightGrid", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addStorageBufferRead("lights", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addStorageBufferRead("meshData", backend::PipelineStage::FRAGMENT_SHADER);
        forwardPass.addIndirectRead("materialCounts");
        forwardPass.addVertexRead("vertexBuffer");
        forwardPass.addIndexRead("indexBuffer");
        forwardPass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

        if (_renderSettings.vxgi)
            forwardPass.addStorageImageRead("voxelGridMipMapped", backend::PipelineStage::FRAGMENT_SHADER);
//        forwardPass.addStorageImageRead("voxelVisualised", backend::PipelineStage::FRAGMENT_SHADER);

        forwardPass.setDebugColour({0.4, 0.1, 0.9, 1});

        forwardPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            auto drawCommands = graph.getBuffer("drawCommands");
            auto lightGrid = graph.getBuffer("lightGrid");
            auto lightIndices = graph.getBuffer("lightIndices");
            auto materialCounts = graph.getBuffer("materialCounts");
            cmd->clearDescriptors();
            if (scene._renderables.empty())
                return;

            cmd->bindBuffer(1, 0, global, true);

            auto& renderable = scene._renderables[0].second.first;

            cmd->bindBindings(renderable.bindings);
            cmd->bindAttributes(renderable.attributes);

            for (u32 i = 0; i < scene._materialCounts.size(); i++) {
                Material* material = &_engine->_materials[i];
                if (!material)
                    continue;
                cmd->bindProgram(material->getVariant(Material::Variant::LIT));
                cmd->bindRasterState(material->getRasterState());
                cmd->bindDepthState(material->getDepthState());
                cmd->bindBuffer(2, 0, material->buffer(), true);

                struct ForwardPush {
                    ende::math::Vec<4, u32> tileSizes;
                    ende::math::Vec<2, u32> screenSize;
                    i32 lightGridIndex;
                    i32 lightIndicesIndex;
                } push;
                push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
                push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
                push.lightGridIndex = lightGrid.index();
                push.lightIndicesIndex = lightIndices.index();

                cmd->pushConstants(backend::ShaderStage::FRAGMENT, push);

                cmd->bindPipeline();
                cmd->bindDescriptors();

                cmd->bindVertexBuffer(0, _engine->_globalVertexBuffer);
                cmd->bindIndexBuffer(_engine->_globalIndexBuffer);

                u32 drawCommandOffset = scene._materialCounts[i].offset * sizeof(VkDrawIndexedIndirectCommand);
                u32 countOffset = i * (sizeof(u32) * 2);
                cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
            }
        });
    }

    if (_renderSettings.tonemap && !fullscreenDebug) {
        auto& tonemapPass = _graph.addPass("tonemap", RenderPass::Type::COMPUTE);

        tonemapPass.addStorageBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
        if (overlayDebug)
            tonemapPass.addStorageImageWrite("backbuffer-debug", backend::PipelineStage::COMPUTE_SHADER);
        else
            tonemapPass.addStorageImageWrite("backbuffer", backend::PipelineStage::COMPUTE_SHADER);
        tonemapPass.addStorageImageRead("hdr", backend::PipelineStage::COMPUTE_SHADER);

        tonemapPass.setDebugColour({0.1, 0.4, 0.7, 1});
        tonemapPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            auto hdrImage = graph.getImage("hdr");
            auto backbuffer = graph.getImage("backbuffer");
            cmd->clearDescriptors();
            cmd->bindProgram(_engine->_tonemapProgram);
            cmd->bindBindings({});
            cmd->bindAttributes({});
            cmd->bindBuffer(1, 0, global, true);
            cmd->bindImage(2, 0, _engine->device().getImageView(hdrImage), _engine->device().defaultSampler(), true);
            cmd->bindImage(2, 1, _engine->device().getImageView(backbuffer), _engine->device().defaultSampler(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->dispatchCompute(std::ceil(backbuffer->width() / 32.f), std::ceil(backbuffer->height() / 32.f), 1);
        });
    }

    if (_renderSettings.skybox && scene._skyLightMap && !fullscreenDebug) {
        auto& skyboxPass = _graph.addPass("skybox");
        if (scene._hdrSkyLight && _renderSettings.tonemap)
            skyboxPass.addColourWrite("hdr");
        else
            skyboxPass.addColourWrite("backbuffer");
        skyboxPass.addDepthRead("depth");
        skyboxPass.setDebugColour({0, 0.7, 0.1, 1});

        skyboxPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

        skyboxPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            cmd->clearDescriptors();
            cmd->bindProgram(_engine->_skyboxProgram);
            cmd->bindRasterState({ backend::CullMode::NONE });
            cmd->bindDepthState({ true, false, backend::CompareOp::LESS_EQUAL });
            cmd->bindBindings({ &_engine->_cube->_binding, 1 });
            cmd->bindAttributes(_engine->_cube->_attributes);
            cmd->bindBuffer(1, 0, global, true);
            i32 skyMapIndex = scene._skyLightMap.index();
            cmd->pushConstants(backend::ShaderStage::FRAGMENT, skyMapIndex);

            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd->bindIndexBuffer(_engine->_globalIndexBuffer);
            cmd->draw(_engine->_cube->indexCount, 1, _engine->_cube->firstIndex, 0);
        });
    }

    if (imGui) {
        auto& uiPass = _graph.addPass("ui");
        uiPass.addColourWrite("backbuffer");
        uiPass.setDebugColour({0.7, 0.1, 0.4, 1});

        uiPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            imGui->render(cmd);
        });
    }


    _graph.setBackbufferDimensions(_swapchain->extent().width, _swapchain->extent().height);
//    _graph.setBackbuffer("backbuffer");
    _graph.setBackbuffer("final-swapchain");
    {
        ImageResource backbufferAttachment;
        backbufferAttachment.format = backend::Format::RGBA8_UNORM;
        backbufferAttachment.matchSwapchain = false;
        _graph.addImageResource("final-swapchain", backbufferAttachment);

        auto& blitPass = _graph.addPass("blit", RenderPass::Type::TRANSFER);
        blitPass.addBlitRead("backbuffer");
        blitPass.addBlitWrite("final-swapchain");

        blitPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto backbuffer = graph.getImage("backbuffer");
            _swapchain->blitImageToFrame(_swapchainFrame.index, cmd, *backbuffer);
        });
    }


    if (!_graph.compile())
        throw std::runtime_error("cyclical graph found");



    _globalData.maxDrawCount = scene._renderables.size();
    _globalData.gpuCulling = _renderSettings.gpuCulling;
    _globalData.swapchainSize = { _swapchain->extent().width, _swapchain->extent().height };

    _globalData.tranformsBufferIndex = scene._modelBuffer[_engine->device().frameIndex()].index();
    _globalData.meshBufferIndex = scene._meshDataBuffer[_engine->device().frameIndex()].index();
    _globalData.lightBufferIndex = scene._lightBuffer[_engine->device().frameIndex()].index();
    _globalData.lightCountBufferIndex = scene._lightCountBuffer[_engine->device().frameIndex()].index();
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

    _globalData.vxgi.projection = ende::math::orthographic<f32>(_renderSettings.voxelBounds.first.x(), _renderSettings.voxelBounds.second.x(), _renderSettings.voxelBounds.first.y(), _renderSettings.voxelBounds.second.y(), _renderSettings.voxelBounds.first.z(), _renderSettings.voxelBounds.second.z());// *
//            (ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(2)) * ende::math::Quaternion({ 1, 0, 0 }, ende::math::rad(2))).toMat();
    _globalData.vxgi.gridDimensions = _renderSettings.voxelGridDimensions;

    auto sceneBounds = _renderSettings.voxelBounds.second - _renderSettings.voxelBounds.first;
    ende::math::Vec3f extents = {
            static_cast<f32>(sceneBounds.x()) / static_cast<f32>(_renderSettings.voxelGridDimensions.x()),
            static_cast<f32>(sceneBounds.y()) / static_cast<f32>(_renderSettings.voxelGridDimensions.y()),
            static_cast<f32>(sceneBounds.z()) / static_cast<f32>(_renderSettings.voxelGridDimensions.z())
    };

    _globalData.vxgi.voxelExtent = extents;
    _globalData.vxgi.gridIndex = _graph.getImage("voxelGrid").index();

    _globalDataBuffer[_engine->device().frameIndex()]->data(std::span<RendererGlobal>(&_globalData, 1));

    cmd->startPipelineStatistics();
    _graph.execute(cmd);
    cmd->stopPipelineStatistics();

}