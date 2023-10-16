#include "debugPasses.h"
#include <Cala/Material.h>

using namespace cala;

void debugNormalPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& normalsPass = graph.addPass("debug_normals");

//    normalsPass.addColourAttachment("backbuffer");
    normalsPass.addColourAttachment("hdr");
    normalsPass.addDepthAttachment("depth");

    normalsPass.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    normalsPass.addIndirectBufferRead("drawCommands");
    normalsPass.addIndirectBufferRead("materialCounts");
    normalsPass.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    normalsPass.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    normalsPass.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    normalsPass.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");

        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::NORMAL);
            if (!program)
                continue;
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}

void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugRoughness = graph.addPass("debug_roughness");

//    debugRoughness.addColourAttachment("backbuffer");
    debugRoughness.addColourAttachment("hdr");
    debugRoughness.addDepthAttachment("depth");

    debugRoughness.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugRoughness.addIndirectBufferRead("drawCommands");
    debugRoughness.addIndirectBufferRead("materialCounts");
    debugRoughness.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugRoughness.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugRoughness.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugRoughness.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::ROUGHNESS);
            if (!program)
                continue;
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}

void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugMetallic = graph.addPass("debug_metallic");

//    debugMetallic.addColourAttachment("backbuffer");
    debugMetallic.addColourAttachment("hdr");
    debugMetallic.addDepthAttachment("depth");

    debugMetallic.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugMetallic.addIndirectBufferRead("drawCommands");
    debugMetallic.addIndirectBufferRead("materialCounts");
    debugMetallic.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugMetallic.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugMetallic.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugMetallic.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::METALLIC);
            if (!program)
                continue;
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}

void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugUnlit = graph.addPass("debug_unlit");

//    debugUnlit.addColourAttachment("backbuffer");
    debugUnlit.addColourAttachment("hdr");
    debugUnlit.addDepthAttachment("depth");

    debugUnlit.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugUnlit.addIndirectBufferRead("drawCommands");
    debugUnlit.addIndirectBufferRead("materialCounts");
    debugUnlit.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugUnlit.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugUnlit.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugUnlit.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd->bindRasterState({});
        cmd->bindVertexBuffer(0, engine.vertexBuffer());
        cmd->bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::UNLIT);
            if (!program)
                continue;
            cmd->bindProgram(program);
            cmd->bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            u32 drawCommandOffset = scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand);
            u32 countOffset = material * (sizeof(u32) * 2);
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}

void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugWorldPos = graph.addPass("debug_worldPos");

//    debugWorldPos.addColourAttachment("backbuffer");
    debugWorldPos.addColourAttachment("hdr");
    debugWorldPos.addDepthAttachment("depth");

    debugWorldPos.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWorldPos.addIndirectBufferRead("drawCommands");
    debugWorldPos.addIndirectBufferRead("materialCounts");
    debugWorldPos.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugWorldPos.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWorldPos.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugWorldPos.setExecuteFunction([&](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
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
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}

void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings, const char* backbuffer) {
    auto& debugWireframe = graph.addPass("debug_wireframe");

    debugWireframe.addColourAttachment(backbuffer);
    debugWireframe.addDepthAttachment("depth");

    debugWireframe.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWireframe.addIndirectBufferRead("drawCommands");
    debugWireframe.addIndirectBufferRead("materialCounts");
    debugWireframe.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugWireframe.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugWireframe.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::GEOMETRY_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugWireframe.setExecuteFunction([settings, &engine, &scene](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
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
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}

void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings, const char* backbuffer) {
    auto& debugNormalLines = graph.addPass("debug_normal_lines");

    debugNormalLines.addColourAttachment(backbuffer);
    debugNormalLines.addDepthAttachment("depth");

    debugNormalLines.addStorageBufferRead("global", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugNormalLines.addIndirectBufferRead("drawCommands");
    debugNormalLines.addIndirectBufferRead("materialCounts");
    debugNormalLines.addStorageBufferRead("transforms", backend::PipelineStage::VERTEX_SHADER);
    debugNormalLines.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::FRAGMENT_SHADER);
    debugNormalLines.addStorageBufferRead("camera", backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::GEOMETRY_SHADER | backend::PipelineStage::FRAGMENT_SHADER);

    debugNormalLines.setExecuteFunction([settings, &engine, &scene](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd->clearDescriptors();
        cmd->bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd->bindBindings(renderable.bindings);
        cmd->bindAttributes(renderable.attributes);
        cmd->bindDepthState({ true, true, cala::backend::CompareOp::LESS });
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
            cmd->drawIndirectCount(drawCommands->handle, drawCommandOffset, materialCounts->handle, countOffset);
        }
    });
}