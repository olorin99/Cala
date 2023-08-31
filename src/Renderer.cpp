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
    _cameraBuffer{engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING),
                  engine->device().createBuffer(sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING)},
    _drawCountBuffer{engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::STAGING),
                     engine->device().createBuffer(sizeof(u32) * 2, backend::BufferUsage::STORAGE | backend::BufferUsage::INDIRECT, backend::MemoryProperties::STAGING)},
    _globalDataBuffer(engine->device().createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING)),
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
    if (!_frameInfo.cmd->submit({ &_swapchainFrame.imageAquired, 1 }, _frameInfo.fence)) {
        _engine->logger().error("Error submitting command buffer");
        _engine->device().printMarkers();
        throw std::runtime_error("Error submitting command buffer");
    }

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

    bool debugViewEnabled = _renderSettings.debugNormals || _renderSettings.debugRoughness || _renderSettings.debugMetallic || _renderSettings.debugWorldPos || _renderSettings.debugUnlit || _renderSettings.debugWireframe || _renderSettings.debugNormalLines;

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
    }



//    ImageResource directDepth;
//    pointDepth.format = backend::Format::D32_SFLOAT;
//    pointDepth.matchSwapchain = false;
//    pointDepth.transient = false;
//    pointDepth.width = 10;
//    pointDepth.height = 10;

    BufferResource cameraResource;
    cameraResource.size = _cameraBuffer[_engine->device().frameIndex()]->size();
    cameraResource.usage = _cameraBuffer[_engine->device().frameIndex()]->usage();
    cameraResource.handle = _cameraBuffer[_engine->device().frameIndex()];

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


    _graph.setBackbuffer("backbuffer");

    if (camera.isDirty()) {
        auto& createClusters = _graph.addPass("create_clusters");

        createClusters.addStorageBufferWrite("clusters");

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

    cullLights.addStorageBufferRead("clusters");
    cullLights.addStorageBufferWrite("lightGrid");
    cullLights.addStorageBufferWrite("lightIndices");
    cullLights.addStorageBufferWrite("lightGlobalResource");

    cullLights.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
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
        } push;
        push.inverseProjection = camera.projection().inverse();
        push.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
        push.screenSize = { _swapchain->extent().width, _swapchain->extent().height };
        push.near = camera.near();
        push.far = camera.far();

        cmd.pushConstants(backend::ShaderStage::COMPUTE, { &push, sizeof(push) });
        cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()], false);
        cmd.bindBuffer(1, 1, clusters->handle, true);
        cmd.bindBuffer(1, 2, scene._lightBuffer[_engine->device().frameIndex()], true);
        cmd.bindBuffer(1, 3, lightGrid->handle, true);
        cmd.bindBuffer(1, 4, lightGlobalIndex->handle, true);
        cmd.bindBuffer(1, 5, lightIndices->handle, true);
        cmd.bindBuffer(1, 6, scene._lightCountBuffer[_engine->device().frameIndex()], true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(1, 1, 6);
    });

    if (_renderSettings.debugClusters) {
        auto& debugClusters = _graph.addPass("debug_clusters");

        debugClusters.addColourAttachment("hdr");
        debugClusters.addStorageBufferRead("lightGrid");
        debugClusters.addSampledImageRead("depth");

        debugClusters.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto lightGrid = graph.getResource<BufferResource>("lightGrid");
            auto depthBuffer = graph.getResource<ImageResource>("depth");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
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
        auto& debugNormals = _graph.addPass("debug_normals");

        debugNormals.addColourAttachment("backbuffer");
        debugNormals.addDepthAttachment("depth");

        debugNormals.addStorageBufferRead("drawCommands");
        debugNormals.addStorageBufferRead("materialCounts");
        debugNormals.addStorageBufferRead("transforms");
        debugNormals.addStorageBufferRead("meshData");

        debugNormals.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({});
            cmd.bindBuffer(2, 1, meshData->handle, true);
            cmd.bindBuffer(4, 0, transforms->handle, true);
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                auto program = _engine->_materials[material].getVariant(Material::Variant::NORMAL);
                if (!program)
                    continue;
                cmd.bindProgram(program);
                cmd.bindBuffer(2, 0, _engine->_materials[material].buffer(), true);
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.debugRoughness) {
        auto& debugRoughness = _graph.addPass("debug_roughness");

        debugRoughness.addColourAttachment("backbuffer");
        debugRoughness.addDepthAttachment("depth");

        debugRoughness.addStorageBufferRead("drawCommands");
        debugRoughness.addStorageBufferRead("materialCounts");
        debugRoughness.addStorageBufferRead("transforms");
        debugRoughness.addStorageBufferRead("meshData");

        debugRoughness.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({});
            cmd.bindBuffer(2, 1, meshData->handle, true);
            cmd.bindBuffer(4, 0, transforms->handle, true);
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                auto program = _engine->_materials[material].getVariant(Material::Variant::ROUGHNESS);
                if (!program)
                    continue;
                cmd.bindProgram(program);
                cmd.bindBuffer(2, 0, _engine->_materials[material].buffer(), true);
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.debugMetallic) {
        auto& debugMetallic = _graph.addPass("debug_metallic");

        debugMetallic.addColourAttachment("backbuffer");
        debugMetallic.addDepthAttachment("depth");

        debugMetallic.addStorageBufferRead("drawCommands");
        debugMetallic.addStorageBufferRead("materialCounts");
        debugMetallic.addStorageBufferRead("transforms");
        debugMetallic.addStorageBufferRead("meshData");

        debugMetallic.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({});
            cmd.bindBuffer(2, 1, meshData->handle, true);
            cmd.bindBuffer(4, 0, transforms->handle, true);
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                auto program = _engine->_materials[material].getVariant(Material::Variant::METALLIC);
                if (!program)
                    continue;
                cmd.bindProgram(program);
                cmd.bindBuffer(2, 0, _engine->_materials[material].buffer(), true);
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.debugUnlit) {
        auto& debugUnlit = _graph.addPass("debug_unlit");

        debugUnlit.addColourAttachment("backbuffer");
        debugUnlit.addDepthAttachment("depth");

        debugUnlit.addStorageBufferRead("drawCommands");
        debugUnlit.addStorageBufferRead("materialCounts");
        debugUnlit.addStorageBufferRead("transforms");
        debugUnlit.addStorageBufferRead("meshData");

        debugUnlit.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({});
            cmd.bindBuffer(2, 1, meshData->handle, true);
            cmd.bindBuffer(4, 0, transforms->handle, true);
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                auto program = _engine->_materials[material].getVariant(Material::Variant::UNLIT);
                if (!program)
                    continue;
                cmd.bindProgram(program);
                cmd.bindBuffer(2, 0, _engine->_materials[material].buffer(), true);
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.debugWorldPos) {
        auto& debugWorldPos = _graph.addPass("debug_worldPos");

        debugWorldPos.addColourAttachment("backbuffer");
        debugWorldPos.addDepthAttachment("depth");

        debugWorldPos.addStorageBufferRead("drawCommands");
        debugWorldPos.addStorageBufferRead("materialCounts");
        debugWorldPos.addStorageBufferRead("transforms");
        debugWorldPos.addStorageBufferRead("meshData");

        debugWorldPos.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
//            cmd.bindProgram(_engine->_normalsDebugProgram);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({});
            cmd.bindBuffer(4, 0, transforms->handle, true);
//            cmd.bindPipeline();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                cmd.bindProgram(_engine->_worldPosDebugProgram);
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.debugWireframe) {
        auto& debugWireframe = _graph.addPass("debug_wireframe");

        debugWireframe.addColourAttachment("backbuffer");
        debugWireframe.addDepthAttachment("depth");

        debugWireframe.addStorageBufferRead("drawCommands");
        debugWireframe.addStorageBufferRead("materialCounts");
        debugWireframe.addStorageBufferRead("transforms");
        debugWireframe.addStorageBufferRead("meshData");

        debugWireframe.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindRasterState({
                .polygonMode = backend::PolygonMode::LINE,
                .lineWidth = _renderSettings.wireframeThickness
            });
            cmd.bindBuffer(4, 0, transforms->handle, true);
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                cmd.bindProgram(_engine->_solidColourProgram);
                cmd.pushConstants(backend::ShaderStage::FRAGMENT, { &_renderSettings.wireframeColour, sizeof(_renderSettings.wireframeColour) });
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
    }

    if (_renderSettings.debugNormalLines) {
        auto& debugNormalLines = _graph.addPass("debug_normal_lines");

        debugNormalLines.addColourAttachment("backbuffer");
        debugNormalLines.addDepthAttachment("depth");

        debugNormalLines.addStorageBufferRead("drawCommands");
        debugNormalLines.addStorageBufferRead("materialCounts");
        debugNormalLines.addStorageBufferRead("transforms");
        debugNormalLines.addStorageBufferRead("meshData");

        debugNormalLines.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto transforms = graph.getResource<BufferResource>("transforms");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindBuffer(4, 0, transforms->handle, true);
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                cmd.bindProgram(_engine->_normalsDebugProgram);
                cmd.pushConstants(backend::ShaderStage::FRAGMENT, { &_renderSettings.wireframeColour, sizeof(_renderSettings.wireframeColour) });
                cmd.pushConstants(backend::ShaderStage::GEOMETRY, { &_renderSettings.normalLength, sizeof(_renderSettings.normalLength) }, sizeof(_renderSettings.wireframeColour));
                cmd.bindPipeline();
                cmd.bindDescriptors();
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
            }
        });
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

    auto& pointShadows = _graph.addPass("point_shadows");

    pointShadows.addSampledImageWrite("pointDepth");
    pointShadows.addStorageBufferRead("transforms");
    pointShadows.addStorageBufferRead("meshData");
    pointShadows.addStorageBufferWrite("shadowDrawCommands");

    pointShadows.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto transforms = graph.getResource<BufferResource>("transforms");
        auto meshData = graph.getResource<BufferResource>("meshData");
        auto drawCommands = graph.getResource<BufferResource>("shadowDrawCommands");
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
                    cmd.bindBuffer(2, 0, transforms->handle, true);
                    cmd.bindBuffer(2, 1, meshData->handle, true);
                    cmd.bindBuffer(2, 2, drawCommands->handle, true);
                    cmd.bindBuffer(2, 3, _drawCountBuffer[_engine->device().frameIndex()], true);
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

                    cmd.bindBuffer(1, 0, scene._modelBuffer[_engine->device().frameIndex()], true);

                    cmd.bindPipeline();
                    cmd.bindDescriptors();
                    cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
                    cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
                    cmd.drawIndirectCount(drawCommands->handle, 0, _drawCountBuffer[_engine->device().frameIndex()], 0, scene._renderables.size());

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


    auto& cullPass = _graph.addPass("cull");

    cullPass.addStorageBufferRead("transforms");
    cullPass.addStorageBufferRead("meshData");
    cullPass.addStorageBufferRead("materialCounts");
    cullPass.addStorageBufferWrite("drawCommands");

    cullPass.setDebugColour({0.3, 0.3, 1, 1});

    cullPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
        auto transforms = graph.getResource<BufferResource>("transforms");
        auto meshData = graph.getResource<BufferResource>("meshData");
        auto materialCounts = graph.getResource<BufferResource>("materialCounts");
        auto drawCommands = graph.getResource<BufferResource>("drawCommands");
        cmd.clearDescriptors();
        cmd.bindProgram(_engine->_cullProgram);
        cmd.bindBindings(nullptr);
        cmd.bindAttributes(nullptr);
        cmd.pushConstants(backend::ShaderStage::COMPUTE, { &_cullingFrustum, sizeof(_cullingFrustum) });
        cmd.bindBuffer(2, 0, transforms->handle, true);
        cmd.bindBuffer(2, 1, meshData->handle, true);
        cmd.bindBuffer(2, 2, drawCommands->handle, true);
        cmd.bindBuffer(2, 3, _drawCountBuffer[_engine->device().frameIndex()], true);
        cmd.bindBuffer(2, 4, materialCounts->handle, true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(std::ceil(scene._renderables.size() / 16.f), 1, 1);
    });


    if (_renderSettings.depthPre) {
        auto& depthPrePass = _graph.addPass("depth_pre");

        depthPrePass.addDepthAttachment("depth");

        depthPrePass.addStorageBufferRead("drawCommands");
        depthPrePass.addStorageBufferRead("materialCounts");

        depthPrePass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            auto& renderable = scene._renderables[0].second.first;

            cmd.bindBindings(renderable.bindings);
            cmd.bindAttributes(renderable.attributes);
            cmd.bindProgram(_engine->_directShadowProgram);
            cmd.bindDepthState({ true, true, backend::CompareOp::LESS });
            cmd.bindBuffer(4, 0, scene._modelBuffer[_engine->device().frameIndex()], true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
            cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
//            cmd.drawIndirectCount(*drawCommands->handle, 0, *_drawCountBuffer[_engine->device().frameIndex()], 0, scene._renderables.size());
            for (u32 material = 0; material < scene._materialCounts.size(); material++) {
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
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

        forwardPass.addSampledImageRead("pointDepth");
        forwardPass.addStorageBufferRead("drawCommands");
        forwardPass.addStorageBufferRead("lightIndices");
        forwardPass.addStorageBufferRead("lightGrid");
        forwardPass.addStorageBufferRead("materialCounts");

        forwardPass.setDebugColour({0.4, 0.1, 0.9, 1});

        forwardPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto meshData = graph.getResource<BufferResource>("meshData");
            auto drawCommands = graph.getResource<BufferResource>("drawCommands");
            auto lightGrid = graph.getResource<BufferResource>("lightGrid");
            auto lightIndices = graph.getResource<BufferResource>("lightIndices");
            auto materialCounts = graph.getResource<BufferResource>("materialCounts");
            cmd.clearDescriptors();
            if (scene._renderables.empty())
                return;

            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
//            cmd.bindBuffer(1, 1, *_globalDataBuffer);

//            if (!scene._lightData.empty()) {
//                cmd.bindBuffer(2, 4, scene._lightBuffer[_engine->device().frameIndex()], true);
//            }

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
                cmd.bindBuffer(2, 1, meshData->handle, true);
                cmd.bindBuffer(2, 2, lightGrid->handle, true);
                cmd.bindBuffer(2, 3, lightIndices->handle, true);
                cmd.bindBuffer(3, 0, scene._lightBuffer[_engine->device().frameIndex()], true);
                cmd.bindBuffer(4, 0, scene._modelBuffer[_engine->device().frameIndex()], true);

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

                cmd.pushConstants(backend::ShaderStage::FRAGMENT, { &push, sizeof(push) });

                cmd.bindPipeline();
                cmd.bindDescriptors();

                cmd.bindVertexBuffer(0, _engine->_globalVertexBuffer);
                cmd.bindIndexBuffer(_engine->_globalIndexBuffer);
                cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[i].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, i * (sizeof(u32) * 2), scene._materialCounts[i].count);
            }
        });
    }

    if (_renderSettings.tonemap && !debugViewEnabled) {
        auto& tonemapPass = _graph.addPass("tonemap");

        tonemapPass.addStorageImageWrite("backbuffer");
        tonemapPass.addStorageImageRead("hdr");

        tonemapPass.setDebugColour({0.1, 0.4, 0.7, 1});
        tonemapPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            auto hdrImage = graph.getResource<ImageResource>("hdr");
            auto backbuffer = graph.getResource<ImageResource>("backbuffer");
            cmd.clearDescriptors();
            cmd.bindProgram(_engine->_tonemapProgram);
            cmd.bindBindings(nullptr);
            cmd.bindAttributes(nullptr);
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            cmd.bindBuffer(1, 1, _globalDataBuffer);
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

        skyboxPass.setExecuteFunction([&](backend::vulkan::CommandBuffer& cmd, RenderGraph& graph) {
            cmd.clearDescriptors();
            cmd.bindProgram(_engine->_skyboxProgram);
            cmd.bindRasterState({ backend::CullMode::NONE });
            cmd.bindDepthState({ true, false, backend::CompareOp::LESS_EQUAL });
            cmd.bindBindings({ &_engine->_cube->_binding, 1 });
            cmd.bindAttributes(_engine->_cube->_attributes);
            cmd.bindBuffer(1, 0, _cameraBuffer[_engine->device().frameIndex()]);
            cmd.bindImage(2, 0, scene._skyLightMapView, _engine->device().defaultSampler());
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
        throw "cyclical graph found";

    cmd.startPipelineStatistics();
    _graph.execute(cmd, _swapchainFrame.index);
    cmd.stopPipelineStatistics();

}