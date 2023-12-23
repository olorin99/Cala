#include "debugPasses.h"
#include <Cala/Material.h>

using namespace cala;
void debugClusters(cala::RenderGraph& graph, cala::Engine& engine, cala::vk::Swapchain& swapchain) {
    auto& debugClusters = graph.addPass("debug_clusters");

    debugClusters.addColourRead("backbuffer-debug");
    debugClusters.addColourWrite("backbuffer");

    debugClusters.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugClusters.addStorageBufferRead("lightGrid", vk::PipelineStage::FRAGMENT_SHADER);
    debugClusters.addSampledImageRead("depth", vk::PipelineStage::FRAGMENT_SHADER);

    debugClusters.setExecuteFunction([&](vk::CommandHandle cmd, RenderGraph& graph) {
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
        cmd->pushConstants(vk::ShaderStage::FRAGMENT, push);
        cmd->bindImage(1, 1, depthBuffer->defaultView(), engine.device().defaultShadowSampler());
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

    normalsPass.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    normalsPass.addIndirectRead("drawCommands");
    normalsPass.addIndirectRead("materialCounts");
    normalsPass.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    normalsPass.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    normalsPass.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    normalsPass.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, drawCommands, true);
        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);
        cmd->bindDepthState({ true, true, cala::vk::CompareOp::LESS_EQUAL });
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

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
    });
}

void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugRoughness = graph.addPass("debug_roughness");

    debugRoughness.addColourWrite("backbuffer");
    debugRoughness.addDepthWrite("depth");

    debugRoughness.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugRoughness.addIndirectRead("drawCommands");
    debugRoughness.addIndirectRead("materialCounts");
    debugRoughness.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugRoughness.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugRoughness.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugRoughness.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, drawCommands, true);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, true, cala::vk::CompareOp::LESS_EQUAL });
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

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
    });
}

void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugMetallic = graph.addPass("debug_metallic");

    debugMetallic.addColourWrite("backbuffer");
    debugMetallic.addDepthWrite("depth");

    debugMetallic.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugMetallic.addIndirectRead("drawCommands");
    debugMetallic.addIndirectRead("materialCounts");
    debugMetallic.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugMetallic.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugMetallic.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugMetallic.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, drawCommands, true);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, true, cala::vk::CompareOp::LESS_EQUAL });
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

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
    });
}

void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugUnlit = graph.addPass("debug_unlit");

    debugUnlit.addColourWrite("backbuffer");
    debugUnlit.addDepthWrite("depth");

    debugUnlit.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugUnlit.addIndirectRead("drawCommands");
    debugUnlit.addIndirectRead("materialCounts");
    debugUnlit.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugUnlit.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugUnlit.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugUnlit.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, drawCommands, true);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, true, cala::vk::CompareOp::LESS_EQUAL });
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

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
    });
}

void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugWorldPos = graph.addPass("debug_worldPos");

    debugWorldPos.addColourWrite("backbuffer");
    debugWorldPos.addDepthWrite("depth");

    debugWorldPos.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugWorldPos.addIndirectRead("drawCommands");
    debugWorldPos.addIndirectRead("materialCounts");
    debugWorldPos.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugWorldPos.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugWorldPos.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugWorldPos.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, drawCommands, true);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, true, cala::vk::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
//            cmd->bindPipeline();
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_WORLDPOS));
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
    });
}

void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
    auto& debugWireframe = graph.addPass("debug_wireframe");

    debugWireframe.addColourRead("backbuffer-debug");
    debugWireframe.addColourWrite("backbuffer");
    debugWireframe.addDepthRead("depth");

    debugWireframe.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugWireframe.addIndirectRead("drawCommands");
    debugWireframe.addIndirectRead("materialCounts");
    debugWireframe.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugWireframe.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugWireframe.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::GEOMETRY_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugWireframe.setExecuteFunction([settings, &engine, &scene](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, false, cala::vk::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({
            .polygonMode = cala::vk::PolygonMode::LINE,
            .lineWidth = settings.wireframeThickness
        });
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::SOLID_COLOUR));
            cmd->pushConstants(cala::vk::ShaderStage::FRAGMENT, settings.wireframeColour);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
    });
}

void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
    auto& debugNormalLines = graph.addPass("debug_normal_lines");

    debugNormalLines.addColourRead("backbuffer-debug");
    debugNormalLines.addColourWrite("backbuffer");
    debugNormalLines.addDepthRead("depth");

    debugNormalLines.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugNormalLines.addIndirectRead("drawCommands");
    debugNormalLines.addIndirectRead("materialCounts");
    debugNormalLines.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugNormalLines.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugNormalLines.addStorageBufferRead("camera", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::GEOMETRY_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugNormalLines.setExecuteFunction([settings, &engine, &scene](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, false, cala::vk::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_NORMALS));
            cmd->pushConstants(cala::vk::ShaderStage::FRAGMENT, settings.wireframeColour);
            cmd->pushConstants(cala::vk::ShaderStage::GEOMETRY, settings.normalLength, sizeof(settings.wireframeColour));
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset);
        }
    });
}

void debugFrustum(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings) {
    auto& debugFrustum = graph.addPass("debug_frustums");

    debugFrustum.addColourRead("backbuffer-debug");
    debugFrustum.addColourWrite("backbuffer");
    debugFrustum.addDepthRead("depth");

    debugFrustum.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugFrustum.setExecuteFunction([settings, &engine](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto cameraBuffer = graph.getBuffer("camera");
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

        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        cmd->draw(engine.unitCube()->indexCount, 1, engine.unitCube()->firstIndex, 0);
    });
}

void debugDepthPass(cala::RenderGraph& graph, cala::Engine& engine) {
    auto& debugDepth = graph.addPass("debug_depth");

    debugDepth.addColourWrite("backbuffer");
    debugDepth.addSampledImageRead("depth", vk::PipelineStage::FRAGMENT_SHADER);

    debugDepth.addUniformBufferRead("global", vk::PipelineStage::VERTEX_SHADER | vk::PipelineStage::FRAGMENT_SHADER);

    debugDepth.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto depth = graph.getImage("depth");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBindings({});
        cmd->bindAttributes({});
        cmd->bindBlendState({ true });
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

void debugMeshletPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugMeshlet = graph.addPass("debug_meshlet");

    debugMeshlet.addColourWrite("backbuffer");
    debugMeshlet.addDepthWrite("depth");

    debugMeshlet.addUniformBufferRead("global", vk::PipelineStage::FRAGMENT_SHADER | vk::PipelineStage::TASK_SHADER | vk::PipelineStage::MESH_SHADER);
    debugMeshlet.addIndirectRead("drawCommands");
    debugMeshlet.addStorageBufferRead("drawCommands", vk::PipelineStage::TASK_SHADER);
    debugMeshlet.addIndirectRead("materialCounts");
    debugMeshlet.addStorageBufferRead("vertexBuffer", vk::PipelineStage::TASK_SHADER | vk::PipelineStage::MESH_SHADER);
    debugMeshlet.addStorageBufferRead("transforms", vk::PipelineStage::VERTEX_SHADER);
    debugMeshlet.addStorageBufferRead("meshData", vk::PipelineStage::TASK_SHADER | vk::PipelineStage::MESH_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugMeshlet.addStorageBufferRead("camera", vk::PipelineStage::TASK_SHADER | vk::PipelineStage::MESH_SHADER | vk::PipelineStage::FRAGMENT_SHADER);
    debugMeshlet.addStorageBufferWrite("feedback", vk::PipelineStage::TASK_SHADER | vk::PipelineStage::MESH_SHADER);

    debugMeshlet.setExecuteFunction([&](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("drawCommands");
        auto materialCounts = graph.getBuffer("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global);
        cmd->bindBuffer(1, 1, drawCommands, true);

        auto binding = engine.globalBinding();
        auto attributes = engine.globalVertexAttributes();
        cmd->bindBindings({ &binding, 1 });
        cmd->bindAttributes(attributes);

        cmd->bindDepthState({ true, true, cala::vk::CompareOp::LESS_EQUAL });
        cmd->bindRasterState({});
//            cmd->bindPipeline();
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_MESHLETS));
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(MeshTaskCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawMeshTasksIndirectCount(drawCommands, drawCommandOffset, materialCounts, countOffset, sizeof(MeshTaskCommand));
        }
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