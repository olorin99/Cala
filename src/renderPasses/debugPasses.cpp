#include "debugPasses.h"
#include <Cala/Material.h>

using namespace cala;
void debugClusters(cala::RenderGraph& graph, cala::Engine& engine, cala::vk::Swapchain& swapchain, ClusterDebugInput input) {
    auto& debugClusters = graph.addPass("debug_clusters");

    debugClusters.addColourRead("backbuffer-debug");
    debugClusters.addColourWrite(input.backbuffer);

    debugClusters.addUniformBufferRead(input.global, vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugClusters.addStorageBufferRead(input.lightGrid, vk::PipelineStage::FRAGMENT_SHADER);
    debugClusters.addSampledImageRead(input.depth, vk::PipelineStage::FRAGMENT_SHADER);

    debugClusters.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto lightGrid = graph.getBuffer(input.lightGrid);
        auto depthBuffer = graph.getImage(input.depth);
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->bindBlendState({ true });
        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_CLUSTER));
        struct ClusterPush {
            u64 lightGridBuffer;
        } push;
        push.lightGridBuffer = lightGrid->address();
        cmd->pushConstants(vk::ShaderStage::FRAGMENT, push);
        cmd->bindImage(1, 1, depthBuffer->defaultView(), engine.device().defaultShadowSampler());
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->draw(3, 1, 0, 0, false);
        cmd->bindBlendState({ false });
    });
}

void debugNormalPass(cala::RenderGraph& graph, cala::Engine& engine, NormalDebugInput input) {
    auto& normalsPass = graph.addPass("debug_normals", RenderPass::Type::COMPUTE);

    normalsPass.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    normalsPass.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    normalsPass.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    normalsPass.addStorageBufferRead(input.vertexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    normalsPass.addStorageBufferRead(input.indexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    normalsPass.addStorageBufferRead(input.transforms, vk::PipelineStage::COMPUTE_SHADER);
    normalsPass.addStorageBufferRead(input.meshData, vk::PipelineStage::COMPUTE_SHADER);
    normalsPass.addStorageBufferRead(input.camera, vk::PipelineStage::COMPUTE_SHADER);

    normalsPass.addStorageImageRead(input.pixelPositions, vk::PipelineStage::COMPUTE_SHADER);
    normalsPass.addIndirectRead(input.dispatchBuffer);

    normalsPass.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);
        auto depth = graph.getImage(input.depth);
        auto materialCounts = graph.getBuffer(input.materialCount);
        auto pixelPositions = graph.getImage(input.pixelPositions);
        auto dispatchCommands = graph.getBuffer(input.dispatchBuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, materialCounts, true);
//        cmd->bindBuffer(1, 2, pixelPositions, true);

        for (u32 i = 0; i < engine.materialCount(); i++) {
            Material* material = engine.getMaterial(i);
            if (!material)
                continue;
            cmd->bindProgram(material->getVariant(Material::Variant::NORMAL));

            cmd->bindBuffer(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING, material->buffer(), true);

            struct Push {
                i32 visibilityImageIndex;
                i32 backbufferIndex;
                i32 depthIndex;
                u32 materialIndex;
                i32 pixelPositionsIndex;
            } push;
            push.visibilityImageIndex = visibilityImage.index();
            push.backbufferIndex = image.index();
            push.depthIndex = depth.index();
            push.materialIndex = i;
            push.pixelPositionsIndex = pixelPositions.index();

            cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 dispatchOffset = i * sizeof(DispatchCommand);
            cmd->dispatchIndirect(dispatchCommands, dispatchOffset);
        }

    });
}

void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, RoughnessDebugInput input) {
    auto& debugRoughness = graph.addPass("debug_roughness", RenderPass::Type::COMPUTE);

    debugRoughness.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugRoughness.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugRoughness.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugRoughness.addStorageBufferRead(input.vertexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugRoughness.addStorageBufferRead(input.indexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugRoughness.addStorageBufferRead(input.transforms, vk::PipelineStage::COMPUTE_SHADER);
    debugRoughness.addStorageBufferRead(input.meshData, vk::PipelineStage::COMPUTE_SHADER);
    debugRoughness.addStorageBufferRead(input.camera, vk::PipelineStage::COMPUTE_SHADER);

    debugRoughness.addStorageImageRead(input.pixelPositions, vk::PipelineStage::COMPUTE_SHADER);
    debugRoughness.addIndirectRead(input.dispatchBuffer);

    debugRoughness.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);
        auto depth = graph.getImage(input.depth);
        auto materialCounts = graph.getBuffer(input.materialCount);
        auto pixelPositions = graph.getImage(input.pixelPositions);
        auto dispatchCommands = graph.getBuffer(input.dispatchBuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, materialCounts, true);
//        cmd->bindBuffer(1, 2, pixelPositions, true);

        for (u32 i = 0; i < engine.materialCount(); i++) {
            Material* material = engine.getMaterial(i);
            if (!material)
                continue;
            cmd->bindProgram(material->getVariant(Material::Variant::ROUGHNESS));

            cmd->bindBuffer(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING, material->buffer(), true);

            struct Push {
                i32 visibilityImageIndex;
                i32 backbufferIndex;
                i32 depthIndex;
                u32 materialIndex;
                i32 pixelPositionsIndex;
            } push;
            push.visibilityImageIndex = visibilityImage.index();
            push.backbufferIndex = image.index();
            push.depthIndex = depth.index();
            push.materialIndex = i;
            push.pixelPositionsIndex = pixelPositions.index();

            cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 dispatchOffset = i * sizeof(DispatchCommand);
            cmd->dispatchIndirect(dispatchCommands, dispatchOffset);
        }

    });
}

void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, MetallicDebugInput input) {
    auto& debugMetallic = graph.addPass("debug_metallic", RenderPass::Type::COMPUTE);

    debugMetallic.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugMetallic.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugMetallic.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugMetallic.addStorageBufferRead(input.vertexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugMetallic.addStorageBufferRead(input.indexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugMetallic.addStorageBufferRead(input.transforms, vk::PipelineStage::COMPUTE_SHADER);
    debugMetallic.addStorageBufferRead(input.meshData, vk::PipelineStage::COMPUTE_SHADER);
    debugMetallic.addStorageBufferRead(input.camera, vk::PipelineStage::COMPUTE_SHADER);

    debugMetallic.addStorageImageRead(input.pixelPositions, vk::PipelineStage::COMPUTE_SHADER);
    debugMetallic.addIndirectRead(input.dispatchBuffer);

    debugMetallic.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);
        auto depth = graph.getImage(input.depth);
        auto materialCounts = graph.getBuffer(input.materialCount);
        auto pixelPositions = graph.getImage(input.pixelPositions);
        auto dispatchCommands = graph.getBuffer(input.dispatchBuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, materialCounts, true);
//        cmd->bindBuffer(1, 2, pixelPositions, true);

        for (u32 i = 0; i < engine.materialCount(); i++) {
            Material* material = engine.getMaterial(i);
            if (!material)
                continue;
            cmd->bindProgram(material->getVariant(Material::Variant::METALLIC));

            cmd->bindBuffer(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING, material->buffer(), true);

            struct Push {
                i32 visibilityImageIndex;
                i32 backbufferIndex;
                i32 depthIndex;
                u32 materialIndex;
                i32 pixelPositionsIndex;
            } push;
            push.visibilityImageIndex = visibilityImage.index();
            push.backbufferIndex = image.index();
            push.depthIndex = depth.index();
            push.materialIndex = i;
            push.pixelPositionsIndex = pixelPositions.index();

            cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 dispatchOffset = i * sizeof(DispatchCommand);
            cmd->dispatchIndirect(dispatchCommands, dispatchOffset);
        }

    });
}

void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, UnlitDebugInput input) {
    auto& debugUnlit = graph.addPass("debug_unlit", RenderPass::Type::COMPUTE);

    debugUnlit.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugUnlit.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugUnlit.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugUnlit.addStorageBufferRead(input.vertexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugUnlit.addStorageBufferRead(input.indexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugUnlit.addStorageBufferRead(input.transforms, vk::PipelineStage::COMPUTE_SHADER);
    debugUnlit.addStorageBufferRead(input.meshData, vk::PipelineStage::COMPUTE_SHADER);
    debugUnlit.addStorageBufferRead(input.camera, vk::PipelineStage::COMPUTE_SHADER);

    debugUnlit.addStorageImageRead(input.pixelPositions, vk::PipelineStage::COMPUTE_SHADER);
    debugUnlit.addIndirectRead(input.dispatchBuffer);

    debugUnlit.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);
        auto depth = graph.getImage(input.depth);
        auto materialCounts = graph.getBuffer(input.materialCount);
        auto pixelPositions = graph.getImage(input.pixelPositions);
        auto dispatchCommands = graph.getBuffer(input.dispatchBuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, materialCounts, true);
//        cmd->bindBuffer(1, 2, pixelPositions, true);

        for (u32 i = 0; i < engine.materialCount(); i++) {
            Material* material = engine.getMaterial(i);
            if (!material)
                continue;
            cmd->bindProgram(material->getVariant(Material::Variant::UNLIT));

            cmd->bindBuffer(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING, material->buffer(), true);

            struct Push {
                i32 visibilityImageIndex;
                i32 backbufferIndex;
                i32 depthIndex;
                u32 materialIndex;
                i32 pixelPositionsIndex;
            } push;
            push.visibilityImageIndex = visibilityImage.index();
            push.backbufferIndex = image.index();
            push.depthIndex = depth.index();
            push.materialIndex = i;
            push.pixelPositionsIndex = pixelPositions.index();

            cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 dispatchOffset = i * sizeof(DispatchCommand);
            cmd->dispatchIndirect(dispatchCommands, dispatchOffset);
        }

    });
}

void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, WorldPosDebugInput input) {
    auto& debugWorldPos = graph.addPass("debug_worldPos", RenderPass::Type::COMPUTE);

    debugWorldPos.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugWorldPos.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugWorldPos.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugWorldPos.addStorageBufferRead(input.vertexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugWorldPos.addStorageBufferRead(input.indexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugWorldPos.addStorageBufferRead(input.transforms, vk::PipelineStage::COMPUTE_SHADER);
    debugWorldPos.addStorageBufferRead(input.meshData, vk::PipelineStage::COMPUTE_SHADER);
    debugWorldPos.addStorageBufferRead(input.camera, vk::PipelineStage::COMPUTE_SHADER);

    debugWorldPos.addIndirectRead(input.dispatchBuffer);

    debugWorldPos.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);
        auto materialCounts = graph.getBuffer(input.materialCount);
        auto dispatchCommands = graph.getBuffer(input.dispatchBuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_WORLDPOS));

        struct Push {
            i32 visibilityImageIndex;
            i32 backbufferIndex;
        } push;
        push.visibilityImageIndex = visibilityImage.index();
        push.backbufferIndex = image.index();

        cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

        cmd->bindPipeline();
        cmd->bindDescriptors();

        cmd->dispatch(visibilityImage->width(), visibilityImage->height(), 1);

    });
}

void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Renderer::Settings settings, WireframeDebugInput input) {
    auto& debugWireframe = graph.addPass("debug_wireframe", RenderPass::Type::COMPUTE);

    debugWireframe.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugWireframe.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugWireframe.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugWireframe.addStorageBufferRead(input.vertexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugWireframe.addStorageBufferRead(input.indexBuffer, vk::PipelineStage::COMPUTE_SHADER);
    debugWireframe.addStorageBufferRead(input.transforms, vk::PipelineStage::COMPUTE_SHADER);
    debugWireframe.addStorageBufferRead(input.meshData, vk::PipelineStage::COMPUTE_SHADER);
    debugWireframe.addStorageBufferRead(input.camera, vk::PipelineStage::COMPUTE_SHADER);

    debugWireframe.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_WIREFRAME));

        struct Push {
            i32 visibilityImageIndex;
            u32 backbufferIndex;
        } push;
        push.visibilityImageIndex = visibilityImage.index();
        push.backbufferIndex = image.index();

        cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

        cmd->bindPipeline();
        cmd->bindDescriptors();

        cmd->dispatch(image->width(), image->height(), 1);
    });
}

//void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
//    auto& debugNormalLines = graph.addPass("debug_normal_lines");
//
//    debugNormalLines.addColourRead("backbuffer-debug");
//    debugNormalLines.addColourWrite("backbuffer");
//    debugNormalLines.addDepthRead("depth");
//
//    debugNormalLines.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
//    debugNormalLines.addIndirectRead("drawCommands");
//    debugNormalLines.addIndirectRead("materialCounts");
//    debugNormalLines.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
//    debugNormalLines.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
//    debugNormalLines.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::GEOMETRY_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
//
//    debugNormalLines.setExecuteFunction([settings, &engine, &scene](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
//        auto global = graph.getBuffer("global");
//        auto drawCommands = graph.getBuffer("drawCommands");
//        auto materialCounts = graph.getBuffer("materialCounts");
//        cmd->clearDescriptors();
//        cmd->bindBuffer(1, 0, global);
//
//        auto binding = engine.globalBinding();
//        auto attributes = engine.globalVertexAttributes();
//        cmd->bindBindings({ &binding, 1 });
//        cmd->bindAttributes(attributes);
//
//        cmd->bindDepthState({ true, false, cala::vk::CompareOp::LESS_EQUAL });
//        cmd->bindRasterState({});
//        cmd->bindVertexBuffer(0, engine.vertexBuffer());
//        cmd->bindIndexBuffer(engine.indexBuffer());
//        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
//            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_NORMALS));
//            cmd->pushConstants(cala::vk::ShaderStage::FRAGMENT, settings.wireframeColour);
//            cmd->pushConstants(cala::vk::ShaderStage::GEOMETRY, settings.normalLength, sizeof(settings.wireframeColour));
//            cmd->bindPipeline();
//            cmd->bindDescriptors();
//
//            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
//            u32 countOffset = material * (sizeof(u32) * 2);
//            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
//        }
//    });
//}

void debugFrustum(cala::RenderGraph& graph, cala::Engine& engine, cala::Renderer::Settings settings, FrustumDebugInput input) {
    auto& debugFrustum = graph.addPass("debug_frustums");

    debugFrustum.addColourRead("backbuffer-debug");
    debugFrustum.addColourWrite(input.backbuffer);
    debugFrustum.addDepthRead(input.depth);

    debugFrustum.addUniformBufferRead(input.global, vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugFrustum.setExecuteFunction([input, settings, &engine](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto cameraBuffer = graph.getBuffer(input.camera);
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->bindDepthState({ true, false, cala::vk::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({
            .cullMode = vk::CullMode::NONE,
            .polygonMode = cala::vk::PolygonMode::LINE,
            .lineWidth = settings.wireframeThickness
        });

        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_FRUSTUM));

        cmd->pushConstants(cala::vk::ShaderStage::FRAGMENT, settings.wireframeColour);

        cmd->bindPipeline();
        cmd->bindDescriptors();

        cmd->draw(engine.unitCube()->indexCount, 1, engine.unitCube()->firstIndex, 0);
    });
}

void debugDepthPass(cala::RenderGraph& graph, cala::Engine& engine, DepthDebugInput input) {
    auto& debugDepth = graph.addPass("debug_depth");

    debugDepth.addColourWrite(input.backbuffer);
    debugDepth.addSampledImageRead(input.depth, vk::PipelineStage::FRAGMENT_SHADER);

    debugDepth.addUniformBufferRead(input.global, vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugDepth.setExecuteFunction([&, input](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto depth = graph.getImage(input.depth);
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_DEPTH));
        struct ClusterPush {
            i32 depthImageIndex;
        } push;
        push.depthImageIndex = depth.index();
        cmd->pushConstants(vk::ShaderStage::FRAGMENT, push);
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->draw(3, 1, 0, 0, false);
    });
}

void debugMeshletPass(cala::RenderGraph& graph, cala::Engine& engine, MeshletDebugInput input) {
    auto& debugMeshlet = graph.addPass("debug_meshlet", RenderPass::Type::COMPUTE);

    debugMeshlet.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugMeshlet.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugMeshlet.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugMeshlet.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_MESHLETS));

        struct Push {
            i32 visibilityImageIndex;
            u32 backbufferIndex;
        } push;
        push.visibilityImageIndex = visibilityImage.index();
        push.backbufferIndex = image.index();

        cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

        cmd->bindPipeline();
        cmd->bindDescriptors();

        cmd->dispatch(image->width(), image->height(), 1);
    });
}

void debugPrimitivePass(cala::RenderGraph& graph, cala::Engine& engine, PrimitiveDebugInput input) {
    auto& debugPrimitive = graph.addPass("debug_primitive", RenderPass::Type::COMPUTE);

    debugPrimitive.addStorageImageWrite(input.backbuffer, vk::PipelineStage::COMPUTE_SHADER);

    debugPrimitive.addUniformBufferRead(input.global, vk::PipelineStage::COMPUTE_SHADER);
    debugPrimitive.addStorageImageRead(input.visbility, vk::PipelineStage::COMPUTE_SHADER);

    debugPrimitive.setExecuteFunction([&, input](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer(input.global);
        auto visibilityImage = graph.getImage(input.visbility);
        vk::ImageHandle image = graph.getImage(input.backbuffer);

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_PRIMITIVES));

        struct Push {
            i32 visibilityImageIndex;
            u32 backbufferIndex;
        } push;
        push.visibilityImageIndex = visibilityImage.index();
        push.backbufferIndex = image.index();

        cmd->pushConstants(vk::ShaderStage::COMPUTE, push);

        cmd->bindPipeline();
        cmd->bindDescriptors();

        cmd->dispatch(image->width(), image->height(), 1);
    });
}

//void debugClusterFrustums(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
//    auto& debugClusterFrustums = graph.addPass("debug_cluster_frustums");
//
//    debugClusterFrustums.addColourRead("backbuffer-debug");
//    debugClusterFrustums.addColourWrite("backbuffer");
//    debugClusterFrustums.addDepthRead("depth");
//
//    debugClusterFrustums.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
//    debugClusterFrustums.addStorageBufferRead("clusters", vk::PipelineStagevkGEOMETRY_SHADER);
//
//    debugClusterFrustums.setExecuteFunction([settings, &engine, &scene](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
//        auto global = graph.getBuffer("global");
//        auto clusters = graph.getBuffer("clusters");
//        cmd->clearDescriptors();
//        cmd->bindBuffer(1, 0, global);
//        auto& renderable = scene._renderables[0].second.first;
//
//        cmd->bindBindings(renderable.bindings);
//        cmd->bindAttributes(renderable.attributes);
//        cmd->bindDepthState({ true, false, cala::vk::CompareOp::LESS_EQUAL });
//        cmd->bindRasterState({});
//
//        cmd->bindProgram(engine.getProgram(Engine::ProgramType::DEBUG_CLUSTER));
//
//        struct Push {
//            u64 clusterBuffer;
//        } push;
//        push.clusterBuffer = clusters->address();
//        cmd->pushConstants(vk::ShaderStage::VERTEX, push);
//
//        cmd->bindPipeline();
//        cmd->bindDescriptors();
//
//        cmd->draw(3, 1, 0, 0, false);
//    });
//}

void debugVxgi(cala::RenderGraph& graph, cala::Engine& engine) {
    auto& debugVoxel = graph.addPass("voxelVisualisation", RenderPass::Type::COMPUTE);

    debugVoxel.addStorageImageWrite("backbuffer", vk::PipelineStage::COMPUTE_SHADER);
//    debugVoxel.addStorageImageRead("voxelGrid", vk::PipelineStage::COMPUTE_SHADER);
    debugVoxel.addSampledImageRead("voxelGridMipMapped", vk::PipelineStage::COMPUTE_SHADER);
    debugVoxel.addUniformBufferRead("global", vk::PipelineStage::COMPUTE_SHADER);
    debugVoxel.addStorageBufferRead("camera", vk::PipelineStage::COMPUTE_SHADER);

    debugVoxel.setExecuteFunction([&engine](vk::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto voxelGrid = graph.getImage("voxelGrid");
        auto backbuffer = graph.getImage("backbuffer");
        cmd->clearDescriptors();
        cmd->bindProgram(engine.getProgram(Engine::ProgramType::VOXEL_VISUALISE));
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->bindBuffer(1, 0, global);
        cmd->bindImage(2, 0, backbuffer->defaultView());
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatch(backbuffer->width(), backbuffer->height(), 1);
    });
}