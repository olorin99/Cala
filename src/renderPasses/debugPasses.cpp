#include "debugPasses.h"
#include <Cala/Material.h>

using namespace cala;

void debugClusters(cala::RenderGraph& graph, cala::Engine& engine, cala::backend::vulkan::Swapchain& swapchain) {
    auto& debugClusters = graph.addPass("debug_clusters");

    debugClusters.addColourRead("backbuffer-debug");
    debugClusters.addColourWrite("backbuffer");

    debugClusters.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugClusters.addStorageBufferRead("lightGrid", backend::PipelineStage::FRAGMENT_SHADER);
    debugClusters.addSampledImageRead("depth", backend::PipelineStage::FRAGMENT_SHADER);

    debugClusters.setExecuteFunction([&](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto lightGrid = graph.getBuffer("lightGrid");
        auto depthBuffer = graph.getImage("depth");
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
        cmd->pushConstants(backend::ShaderStage::FRAGMENT, push);
        cmd->bindImage(1, 1, engine.device().getImageView(depthBuffer), engine.device().defaultShadowSampler());
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->draw(3, 1, 0, 0, false);
        cmd->bindBlendState({ false });
    });
}

void debugNormalPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& normalsPass = graph.addPass("debug_normals");

    normalsPass.addColourWrite("backbuffer");
    normalsPass.addDepthWrite("depth");

    normalsPass.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    normalsPass.addIndirectRead("drawCommands");
    normalsPass.addIndirectRead("materialCounts");
    normalsPass.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    normalsPass.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    normalsPass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    normalsPass.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            if (!engine.getMaterial(material)->getVariant(cala::Material::Variant::NORMAL))
                continue;
            auto& program = engine.getMaterial(material)->getVariant(cala::Material::Variant::NORMAL);
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugRoughness = graph.addPass("debug_roughness");

    debugRoughness.addColourWrite("backbuffer");
    debugRoughness.addDepthWrite("depth");

    debugRoughness.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugRoughness.addIndirectRead("drawCommands");
    debugRoughness.addIndirectRead("materialCounts");
    debugRoughness.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugRoughness.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugRoughness.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugRoughness.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            if (!engine.getMaterial(material)->getVariant(cala::Material::Variant::ROUGHNESS))
                continue;
            auto& program = engine.getMaterial(material)->getVariant(cala::Material::Variant::ROUGHNESS);
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugMetallic = graph.addPass("debug_metallic");

    debugMetallic.addColourWrite("backbuffer");
    debugMetallic.addDepthWrite("depth");

    debugMetallic.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugMetallic.addIndirectRead("drawCommands");
    debugMetallic.addIndirectRead("materialCounts");
    debugMetallic.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugMetallic.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugMetallic.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugMetallic.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            if (!engine.getMaterial(material)->getVariant(cala::Material::Variant::METALLIC))
                continue;
            auto& program = engine.getMaterial(material)->getVariant(cala::Material::Variant::METALLIC);
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugUnlit = graph.addPass("debug_unlit");

    debugUnlit.addColourWrite("backbuffer");
    debugUnlit.addDepthWrite("depth");

    debugUnlit.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugUnlit.addIndirectRead("drawCommands");
    debugUnlit.addIndirectRead("materialCounts");
    debugUnlit.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugUnlit.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugUnlit.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugUnlit.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            if (!engine.getMaterial(material)->getVariant(cala::Material::Variant::UNLIT))
                continue;
            auto& program = engine.getMaterial(material)->getVariant(cala::Material::Variant::UNLIT);
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugWorldPos = graph.addPass("debug_worldPos");

    debugWorldPos.addColourWrite("backbuffer");
    debugWorldPos.addDepthWrite("depth");

    debugWorldPos.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWorldPos.addIndirectRead("drawCommands");
    debugWorldPos.addIndirectRead("materialCounts");
    debugWorldPos.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugWorldPos.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWorldPos.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugWorldPos.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
//            cmd->bindPipeline();
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_WORLDPOS));
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
    auto& debugWireframe = graph.addPass("debug_wireframe");

    debugWireframe.addColourRead("backbuffer-debug");
    debugWireframe.addColourWrite("backbuffer");
    debugWireframe.addDepthRead("depth");

    debugWireframe.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWireframe.addIndirectRead("drawCommands");
    debugWireframe.addIndirectRead("materialCounts");
    debugWireframe.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugWireframe.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWireframe.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::GEOMETRY_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugWireframe.setExecuteFunction([settings, &engine, &scene](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, false, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({
            .polygonMode = cala::backend::PolygonMode::LINE,
            .lineWidth = settings.wireframeThickness
        });
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::SOLID_COLOUR));
            cmd->pushConstants(cala::backend::ShaderStage::FRAGMENT, settings.wireframeColour);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
    auto& debugNormalLines = graph.addPass("debug_normal_lines");

    debugNormalLines.addColourRead("backbuffer-debug");
    debugNormalLines.addColourWrite("backbuffer");
    debugNormalLines.addDepthRead("depth");

    debugNormalLines.addUniformBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugNormalLines.addIndirectRead("drawCommands");
    debugNormalLines.addIndirectRead("materialCounts");
    debugNormalLines.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugNormalLines.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugNormalLines.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::GEOMETRY_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugNormalLines.setExecuteFunction([settings, &engine, &scene](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, false, cala::backend::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_NORMALS));
            cmd->pushConstants(cala::backend::ShaderStage::FRAGMENT, settings.wireframeColour);
            cmd->pushConstants(cala::backend::ShaderStage::GEOMETRY, settings.normalLength, sizeof(settings.wireframeColour));
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugVxgi(cala::RenderGraph& graph, cala::Engine& engine) {
    auto& debugVoxel = graph.addPass("voxelVisualisation", RenderPass::Type::COMPUTE);

    debugVoxel.addStorageImageWrite("backbuffer", backend::PipelineStage::COMPUTE_SHADER);
//    debugVoxel.addStorageImageRead("voxelGrid", backend::PipelineStage::COMPUTE_SHADER);
    debugVoxel.addSampledImageRead("voxelGridMipMapped", backend::PipelineStage::COMPUTE_SHADER);
    debugVoxel.addUniformBufferRead("global", backend::PipelineStage::COMPUTE_SHADER);
    debugVoxel.addStorageBufferRead("camera", backend::PipelineStage::COMPUTE_SHADER);

    debugVoxel.setExecuteFunction([&engine](backend::vulkan::CommandHandle cmd, RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto voxelGrid = graph.getImage("voxelGrid");
        auto backbuffer = graph.getImage("backbuffer");
        cmd->clearDescriptors();
        cmd->bindProgram(engine.getProgram(Engine::ProgramType::VOXEL_VISUALISE));
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->bindBuffer(1, 0, global);
        cmd->bindImage(2, 0, engine.device().getImageView(backbuffer), engine.device().defaultSampler(), true);
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatchCompute(std::ceil(backbuffer->width() / 32.f), std::ceil(backbuffer->height() / 32.f), 1);
    });
}