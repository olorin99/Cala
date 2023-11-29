#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/Probe.h>
#include <Cala/Material.h>
#include <Cala/ImGuiContext.h>
#include <Ende/profile/profile.h>
#include <Ende/thread/thread.h>

#include "renderPasses/debugPasses.h"
#include "renderPasses/shadowPasses.h"
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
        buffer = engine->device().createBuffer(sizeof(Camera::Data) * 10, backend::BufferUsage::UNIFORM, backend::MemoryProperties::DEVICE);
    }
    for (auto& buffer : _globalDataBuffer) {
        buffer = engine->device().createBuffer(sizeof(RendererGlobal), backend::BufferUsage::UNIFORM, backend::MemoryProperties::DEVICE);
    }
    for (auto& buffer : _frustumBuffer) {
        buffer = engine->device().createBuffer(sizeof(ende::math::Vec4f) * 8, backend::BufferUsage::UNIFORM, backend::MemoryProperties::DEVICE);
    }
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
    _engine->gc();
    _frameInfo.cmd->end();
    if (_engine->device().usingTimeline()) {
        u64 waitValue = _engine->device().getFrameValue(_engine->device().prevFrameIndex());
        u64 signalValue = _engine->device().getTimelineSemaphore().increment();
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
    if (!_renderSettings.freezeFrustum) {
        _cullingFrustum = camera.frustum();
        _frustumCorners = camera.getFrustumCorners();
    }
    if ((scene._lights.size() + 1) * sizeof(Camera::Data) > _cameraBuffer[_engine->device().frameIndex()]->size()) {
        _cameraBuffer[_engine->device().frameIndex()] = _engine->device().resizeBuffer(_cameraBuffer[_engine->device().frameIndex()], (scene._lights.size() + 1) * sizeof(Camera::Data));
        std::string debugLabel = "CameraBuffer: " + std::to_string(_engine->device().frameIndex());
        _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_cameraBuffer[_engine->device().frameIndex()]->buffer(), debugLabel);
    }


    auto cameraData = camera.data();
    _engine->stageData(_cameraBuffer[_engine->device().frameIndex()], cameraData);
    _engine->stageData(_frustumBuffer[_engine->device().frameIndex()], std::span<const u8>((u8*)_frustumCorners.data(), 8 * sizeof(ende::math::Vec4f)));

    for (u32 i = 0; i < scene._lights.size(); i++) {
        auto& light = scene._lights[i];
        auto cameraData = light.second.camera().data();
        _engine->stageData(_cameraBuffer[_engine->device().frameIndex()], cameraData, sizeof(cameraData) * (i + 1));
    }

    bool overlayDebug = _renderSettings.debugWireframe || _renderSettings.debugNormalLines || _renderSettings.debugClusters || _renderSettings.debugFrustum || _renderSettings.debugDepth;
    bool fullscreenDebug = _renderSettings.debugNormals || _renderSettings.debugWorldPos || _renderSettings.debugUnlit || _renderSettings.debugMetallic || _renderSettings.debugRoughness;
    bool debugViewEnabled = overlayDebug || fullscreenDebug;

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
            drawCommandsResource.size = scene.meshCount() * sizeof(VkDrawIndexedIndirectCommand);
            drawCommandsResource.usage = backend::BufferUsage::INDIRECT;
            _graph.addBufferResource("drawCommands", drawCommandsResource);

            _graph.addBufferResource("shadowDrawCommands", drawCommandsResource);
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
        transformsResource.size = scene._meshTransformsBuffer[_engine->device().frameIndex()]->size();
        transformsResource.usage = scene._meshTransformsBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("transforms", transformsResource, scene._meshTransformsBuffer[_engine->device().frameIndex()]);
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
    {
        BufferResource frustumBufferResource;
        frustumBufferResource.size = _frustumBuffer[_engine->device().frameIndex()]->size();
        frustumBufferResource.usage = _frustumBuffer[_engine->device().frameIndex()]->usage();
        _graph.addBufferResource("frustumBuffer", frustumBufferResource, _frustumBuffer[_engine->device().frameIndex()]);
    }



    if (camera.isDirty()) {
        auto& createClusters = _graph.addPass("create_clusters", RenderPass::Type::COMPUTE);

        createClusters.addStorageBufferWrite("clusters", backend::PipelineStage::COMPUTE_SHADER);
        createClusters.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

        createClusters.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto clusters = graph.getBuffer("clusters");
            cmd->clearDescriptors();
            cmd->bindProgram(_engine->getProgram(Engine::ProgramType::CREATE_CLUSTERS));
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

    cullLights.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
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
        cmd->bindProgram(_engine->getProgram(Engine::ProgramType::CULL_LIGHTS));
        cmd->bindBindings({});
        cmd->bindAttributes({});
        struct ClusterPush {
            ende::math::Mat4f inverseProjection;
            f32 near;
            f32 far;
            u64 lightGridBuffer;
            u64 lightIndicesBuffer;
        } push;
        push.inverseProjection = camera.projection().inverse();
        push.near = camera.near();
        push.far = camera.far();
        push.lightGridBuffer = lightGrid->address();
        push.lightIndicesBuffer = lightIndices->address();

        cmd->pushConstants(backend::ShaderStage::COMPUTE, push);
        cmd->bindBuffer(1, 0, global);
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

    if (_renderSettings.debugFrustum) {
        debugFrustum(_graph, *_engine, scene, _renderSettings);
    }

    if (_renderSettings.debugDepth) {
        debugDepthPass(_graph, *_engine);
    }


    shadowPoint(_graph, *_engine, scene);



    auto& cullPass = _graph.addPass("cull", RenderPass::Type::COMPUTE);

    cullPass.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
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
        cmd->bindProgram(_engine->getProgram(Engine::ProgramType::CULL));
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->pushConstants(backend::ShaderStage::COMPUTE, _cullingFrustum);
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(2, 0, drawCommands, true);
        cmd->bindBuffer(2, 1, materialCounts, true);
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatchCompute(std::ceil(scene.meshCount() / 16.f), 1, 1);
    });


    if (_renderSettings.depthPre) {
        auto& depthPrePass = _graph.addPass("depth_pre");

        depthPrePass.addDepthWrite("depth");

        depthPrePass.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
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
            cmd->bindBuffer(1, 0, global);

            auto binding = _engine->globalBinding();
            auto attributes = _engine->globalVertexAttributes();
            cmd->bindBindings({ &binding, 1 });
            cmd->bindAttributes(attributes);

            cmd->bindProgram(_engine->getProgram(Engine::ProgramType::SHADOW_DIRECT));
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

        forwardPass.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
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

        forwardPass.setDebugColour({0.4, 0.1, 0.9, 1});

        forwardPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            auto drawCommands = graph.getBuffer("drawCommands");
            auto lightGrid = graph.getBuffer("lightGrid");
            auto lightIndices = graph.getBuffer("lightIndices");
            auto materialCounts = graph.getBuffer("materialCounts");
            cmd->clearDescriptors();
            if (scene.meshCount() == 0)
                return;

            cmd->bindBuffer(1, 0, global);

            auto binding = _engine->globalBinding();
            auto attributes = _engine->globalVertexAttributes();
            cmd->bindBindings({ &binding, 1 });
            cmd->bindAttributes(attributes);

            for (u32 i = 0; i < scene._materialCounts.size(); i++) {
                Material* material = &_engine->_materials[i];
                if (!material)
                    continue;
                cmd->bindProgram(material->getVariant(Material::Variant::LIT));
                cmd->bindRasterState(material->getRasterState());
                cmd->bindDepthState(material->getDepthState());
                cmd->bindBuffer(2, 0, material->buffer(), true);

                struct ForwardPush {
                    u64 lightGridBuffer;
                    u64 lightIndicesBuffer;
                } push;
                push.lightGridBuffer = lightGrid->address();
                push.lightIndicesBuffer = lightIndices->address();

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

    if (_renderSettings.bloom && !fullscreenDebug) {
        {
            /*
                TODO: need better way to handle mip maps and binding them etc
                might need to allocate ranges of indices in bindless table for
                mip maps
            */
            ImageResource bloomDownsampleImage;
            bloomDownsampleImage.matchSwapchain = false;
            bloomDownsampleImage.format = backend::Format::RGBA32_SFLOAT;
            bloomDownsampleImage.width = _swapchain->extent().width / 2;
            bloomDownsampleImage.height = _swapchain->extent().height / 2;
            _graph.addImageResource("bloomDownsample-0", bloomDownsampleImage);
            bloomDownsampleImage.width = bloomDownsampleImage.width / 2;
            bloomDownsampleImage.height = bloomDownsampleImage.height / 2;
            _graph.addImageResource("bloomDownsample-1", bloomDownsampleImage);
            bloomDownsampleImage.width = bloomDownsampleImage.width / 2;
            bloomDownsampleImage.height = bloomDownsampleImage.height / 2;
            _graph.addImageResource("bloomDownsample-2", bloomDownsampleImage);
            bloomDownsampleImage.width = bloomDownsampleImage.width / 2;
            bloomDownsampleImage.height = bloomDownsampleImage.height / 2;
            _graph.addImageResource("bloomDownsample-3", bloomDownsampleImage);
            bloomDownsampleImage.width = bloomDownsampleImage.width / 2;
            bloomDownsampleImage.height = bloomDownsampleImage.height / 2;
            _graph.addImageResource("bloomDownsample-4", bloomDownsampleImage);
            bloomDownsampleImage.width = bloomDownsampleImage.width / 2;
            bloomDownsampleImage.height = bloomDownsampleImage.height / 2;

            ImageResource bloomUpsampleImage;
            bloomUpsampleImage.matchSwapchain = false;
            bloomUpsampleImage.format = backend::Format::RGBA32_SFLOAT;
            bloomUpsampleImage.width = _swapchain->extent().width;
            bloomUpsampleImage.height = _swapchain->extent().height;
            _graph.addImageResource("bloomUpsample-0", bloomUpsampleImage);
            bloomUpsampleImage.width = bloomUpsampleImage.width / 2;
            bloomUpsampleImage.height = bloomUpsampleImage.height / 2;
            _graph.addImageResource("bloomUpsample-1", bloomUpsampleImage);
            bloomUpsampleImage.width = bloomUpsampleImage.width / 2;
            bloomUpsampleImage.height = bloomUpsampleImage.height / 2;
            _graph.addImageResource("bloomUpsample-2", bloomUpsampleImage);
            bloomUpsampleImage.width = bloomUpsampleImage.width / 2;
            bloomUpsampleImage.height = bloomUpsampleImage.height / 2;
            _graph.addImageResource("bloomUpsample-3", bloomUpsampleImage);
            bloomUpsampleImage.width = bloomUpsampleImage.width / 2;
            bloomUpsampleImage.height = bloomUpsampleImage.height / 2;
            _graph.addImageResource("bloomUpsample-4", bloomUpsampleImage); // downsample-4 -> upsample-4

            ImageResource bloomFinalImage;
            bloomFinalImage.format = backend::Format::RGBA32_SFLOAT;
            _graph.addImageResource("bloomFinal", bloomFinalImage);
        }


        {
            auto &bloomDownsamplePass = _graph.addPass("bloom-downsample", RenderPass::Type::COMPUTE);

            bloomDownsamplePass.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
            bloomDownsamplePass.addSampledImageRead("hdr", backend::PipelineStage::COMPUTE_SHADER);
            bloomDownsamplePass.addStorageImageWrite("bloomDownsample-0", backend::PipelineStage::COMPUTE_SHADER);
            bloomDownsamplePass.addStorageImageWrite("bloomDownsample-1", backend::PipelineStage::COMPUTE_SHADER);
            bloomDownsamplePass.addStorageImageWrite("bloomDownsample-2", backend::PipelineStage::COMPUTE_SHADER);
            bloomDownsamplePass.addStorageImageWrite("bloomDownsample-3", backend::PipelineStage::COMPUTE_SHADER);
            bloomDownsamplePass.addStorageImageWrite("bloomDownsample-4", backend::PipelineStage::COMPUTE_SHADER);

            bloomDownsamplePass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph &graph) {
                auto global = graph.getBuffer("global");
                auto hdrImage = graph.getImage("hdr");
                backend::vulkan::ImageHandle downsample[5] = {
                        graph.getImage("bloomDownsample-0"),
                        graph.getImage("bloomDownsample-1"),
                        graph.getImage("bloomDownsample-2"),
                        graph.getImage("bloomDownsample-3"),
                        graph.getImage("bloomDownsample-4")
                };

                cmd->clearDescriptors();

                cmd->bindProgram(_engine->getProgram(Engine::ProgramType::BLOOM_DOWNSAMPLE));
                cmd->bindBuffer(1, 0, global);


                for (i32 mip = 0; mip < 5; mip++) {
                    backend::vulkan::ImageHandle inputImage;
                    if (mip == 0)
                        inputImage = hdrImage;
                    else
                        inputImage = downsample[mip - 1];
                    backend::vulkan::ImageHandle outputImage = downsample[mip];

                    struct Push {
                        i32 inputIndex;
                        i32 outputIndex;
                        i32 bilinearSampler;
                        i32 mipLevel;
                    } push;
                    push.inputIndex = inputImage.index();
                    push.outputIndex = outputImage.index();
                    push.bilinearSampler = _engine->device().getSampler({
                        .filter = VK_FILTER_LINEAR,
                        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                    }).index();
                    push.mipLevel = mip;
                    cmd->pushConstants(backend::ShaderStage::COMPUTE, push);

                    cmd->bindPipeline();
                    cmd->bindDescriptors();
                    cmd->dispatchCompute(std::ceil(inputImage->width() / 32.f), std::ceil(inputImage->height() / 32.f), 1);

                    if (mip != 4) {
                        auto outputBarrier = outputImage->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::COMPUTE_SHADER, backend::Access::SHADER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
                        cmd->pipelineBarrier({ &outputBarrier, 1 });
                    }
                }

            });
        }
        {
            auto& bloomUpsamplePass = _graph.addPass("bloom-upsample", RenderPass::Type::COMPUTE);

            bloomUpsamplePass.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);

            bloomUpsamplePass.addSampledImageRead("bloomDownsample-4", backend::PipelineStage::COMPUTE_SHADER);

            bloomUpsamplePass.addStorageImageWrite("bloomUpsample-0", backend::PipelineStage::COMPUTE_SHADER);
            bloomUpsamplePass.addStorageImageWrite("bloomUpsample-1", backend::PipelineStage::COMPUTE_SHADER);
            bloomUpsamplePass.addStorageImageWrite("bloomUpsample-2", backend::PipelineStage::COMPUTE_SHADER);
            bloomUpsamplePass.addStorageImageWrite("bloomUpsample-3", backend::PipelineStage::COMPUTE_SHADER);
            bloomUpsamplePass.addStorageImageWrite("bloomUpsample-4", backend::PipelineStage::COMPUTE_SHADER);

            bloomUpsamplePass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
                auto global = graph.getBuffer("global");
                backend::vulkan::ImageHandle downsample[5] = {
                        graph.getImage("bloomDownsample-0"),
                        graph.getImage("bloomDownsample-1"),
                        graph.getImage("bloomDownsample-2"),
                        graph.getImage("bloomDownsample-3"),
                        graph.getImage("bloomDownsample-4")
                };
                backend::vulkan::ImageHandle upsample[5] = {
                        graph.getImage("bloomUpsample-0"),
                        graph.getImage("bloomUpsample-1"),
                        graph.getImage("bloomUpsample-2"),
                        graph.getImage("bloomUpsample-3"),
                        graph.getImage("bloomUpsample-4")
                };

                cmd->clearDescriptors();

                cmd->bindProgram(_engine->getProgram(Engine::ProgramType::BLOOM_UPSAMPLE));
                cmd->bindBuffer(1, 0, global);

                for (i32 mip = 4; mip > -1; mip--) {
                    backend::vulkan::ImageHandle inputImage;
                    backend::vulkan::ImageHandle sumImage;
                    if (mip == 4) {
                        inputImage = downsample[4];
                        sumImage = {};
                    } else {
                        inputImage = upsample[mip + 1];
                        sumImage = downsample[mip];
                    }
                    backend::vulkan::ImageHandle outputImage = upsample[mip];

                    struct Push {
                        i32 inputIndex;
                        i32 sumIndex;
                        i32 outputIndex;
                        i32 bilinearSampler;
                    } push;
                    push.inputIndex = inputImage.index();
                    push.sumIndex = sumImage.index();
                    push.outputIndex = outputImage.index();
                    push.bilinearSampler = _engine->device().getSampler({
                        .filter = VK_FILTER_LINEAR,
                        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                    }).index();
                    cmd->pushConstants(backend::ShaderStage::COMPUTE, push);

                    cmd->bindPipeline();
                    cmd->bindDescriptors();
                    cmd->dispatchCompute(std::ceil(outputImage->width() / 32.f), std::ceil(outputImage->height() / 32.f), 1);

                    if (mip != 0) {
                        auto outputBarrier = outputImage->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::COMPUTE_SHADER, backend::Access::SHADER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
                        cmd->pipelineBarrier({ &outputBarrier, 1 });
                    }
                }

            });
        }
        {
            auto& bloomCompositePass = _graph.addPass("bloom-composite", RenderPass::Type::COMPUTE);

            bloomCompositePass.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);

            bloomCompositePass.addStorageImageRead("hdr", backend::PipelineStage::COMPUTE_SHADER);
            bloomCompositePass.addStorageImageRead("bloomUpsample-0", backend::PipelineStage::COMPUTE_SHADER);
            bloomCompositePass.addStorageImageWrite("bloomFinal", backend::PipelineStage::COMPUTE_SHADER);

            bloomCompositePass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
                auto global = graph.getBuffer("global");
                auto upsample = graph.getImage("bloomUpsample-0");
                auto hdr = graph.getImage("hdr");
                auto final = graph.getImage("bloomFinal");

                cmd->clearDescriptors();

                cmd->bindProgram(_engine->getProgram(Engine::ProgramType::BLOOM_COMPOSITE));
                cmd->bindBuffer(1, 0, global);

                struct Push {
                    i32 upsampleIndex;
                    i32 hdrIndex;
                    i32 finalIndex;
                } push;
                push.upsampleIndex = upsample.index();
                push.hdrIndex = hdr.index();
                push.finalIndex = final.index();
                cmd->pushConstants(backend::ShaderStage::COMPUTE, push);

                cmd->bindPipeline();
                cmd->bindDescriptors();
                cmd->dispatchCompute(std::ceil(final->width() / 32.f), std::ceil(final->height() / 32.f), 1);
            });
        }
    }

    if (_renderSettings.tonemap && !fullscreenDebug) {
        auto& tonemapPass = _graph.addPass("tonemap", RenderPass::Type::COMPUTE);

        tonemapPass.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
        if (overlayDebug)
            tonemapPass.addStorageImageWrite("backbuffer-debug", backend::PipelineStage::COMPUTE_SHADER);
        else
            tonemapPass.addStorageImageWrite("backbuffer", backend::PipelineStage::COMPUTE_SHADER);

        if (_renderSettings.bloom)
            tonemapPass.addStorageImageRead("bloomFinal", backend::PipelineStage::COMPUTE_SHADER);
        else
            tonemapPass.addStorageImageRead("hdr", backend::PipelineStage::COMPUTE_SHADER);


        tonemapPass.addStorageImageRead("bloomUpsample-0", backend::PipelineStage::COMPUTE_SHADER);

        tonemapPass.setDebugColour({0.1, 0.4, 0.7, 1});
        tonemapPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            backend::vulkan::ImageHandle hdrImage;
            if (_renderSettings.bloom)
                hdrImage = graph.getImage("bloomFinal");
            else
                hdrImage = graph.getImage("hdr");
            auto backbuffer = graph.getImage("backbuffer");
            cmd->clearDescriptors();
            cmd->bindProgram(_engine->getProgram(Engine::ProgramType::TONEMAP));
            cmd->bindBindings({});
            cmd->bindAttributes({});
            cmd->bindBuffer(1, 0, global);
            cmd->bindImage(2, 0, _engine->device().getImageView(hdrImage));
            cmd->bindImage(2, 1, _engine->device().getImageView(backbuffer));

            struct Push {
                i32 type;
            } push;
            push.type = _renderSettings.tonemapType;
            cmd->pushConstants(backend::ShaderStage::COMPUTE, push);

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

        skyboxPass.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

        skyboxPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto global = graph.getBuffer("global");
            cmd->clearDescriptors();
            cmd->bindProgram(_engine->getProgram(Engine::ProgramType::SKYBOX));
            cmd->bindRasterState({ backend::CullMode::NONE });
            cmd->bindDepthState({ true, false, backend::CompareOp::LESS_EQUAL });
            cmd->bindBindings({ &_engine->_cube->_binding, 1 });
            cmd->bindAttributes(_engine->_cube->_attributes);
            cmd->bindBuffer(1, 0, global);
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
        ImageResource backbufferAttachment;
        backbufferAttachment.format = backend::Format::RGBA8_UNORM;
        _graph.addImageResource("ui-backbuffer", backbufferAttachment);

        auto& uiPass = _graph.addPass("ui");
        uiPass.addSampledImageRead("backbuffer", backend::PipelineStage::FRAGMENT_SHADER);
        uiPass.addColourWrite("ui-backbuffer");
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
//        blitPass.addBlitRead("backbuffer");
        blitPass.addBlitRead("ui-backbuffer");
        blitPass.addBlitWrite("final-swapchain");

        blitPass.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
            auto backbuffer = graph.getImage("ui-backbuffer");
            _swapchain->blitImageToFrame(_swapchainFrame.index, cmd, *backbuffer);
        });
    }


    if (!_graph.compile())
        throw std::runtime_error("cyclical graph found");



    _globalData.maxDrawCount = scene.meshCount();
    _globalData.gpuCulling = _renderSettings.gpuCulling;
    _globalData.tileSizes = { 16, 9, 24, (u32)std::ceil((f32)_swapchain->extent().width / (f32)16.f) };
    _globalData.swapchainSize = { _swapchain->extent().width, _swapchain->extent().height };
    _globalData.bloomStrength = _renderSettings.bloomStrength;

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

    _globalData.meshBuffer = scene._meshDataBuffer[_engine->device().frameIndex()]->address();
    _globalData.transformsBuffer = scene._meshTransformsBuffer[_engine->device().frameIndex()]->address();
    _globalData.cameraBuffer = _cameraBuffer[_engine->device().frameIndex()]->address();
    _globalData.lightBuffer = scene._lightBuffer[_engine->device().frameIndex()]->address();
    _globalData.lightCountBuffer = scene._lightCountBuffer[_engine->device().frameIndex()]->address();

    _engine->stageData(_globalDataBuffer[_engine->device().frameIndex()], _globalData);

    cmd->startPipelineStatistics();
    _graph.execute(cmd);
    cmd->stopPipelineStatistics();

}