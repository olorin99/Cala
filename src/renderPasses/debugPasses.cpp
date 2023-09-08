#include "debugPasses.h"
#include <Cala/Material.h>

void debugNormalPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& normalsPass = graph.addPass("debug_normals");

    normalsPass.addColourAttachment("backbuffer");
    normalsPass.addDepthAttachment("depth");

    normalsPass.addStorageBufferRead("global");
    normalsPass.addStorageBufferRead("drawCommands");
    normalsPass.addStorageBufferRead("materialCounts");
    normalsPass.addStorageBufferRead("transforms");
    normalsPass.addStorageBufferRead("meshData");

    normalsPass.setExecuteFunction([&](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");

        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({});
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::NORMAL);
            if (!program)
                continue;
            cmd.bindProgram(program);
            cmd.bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}

void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugRoughness = graph.addPass("debug_roughness");

    debugRoughness.addColourAttachment("backbuffer");
    debugRoughness.addDepthAttachment("depth");

    debugRoughness.addStorageBufferRead("global");
    debugRoughness.addStorageBufferRead("drawCommands");
    debugRoughness.addStorageBufferRead("materialCounts");
    debugRoughness.addStorageBufferRead("transforms");
    debugRoughness.addStorageBufferRead("meshData");

    debugRoughness.setExecuteFunction([&](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({});
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::ROUGHNESS);
            if (!program)
                continue;
            cmd.bindProgram(program);
            cmd.bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}

void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugMetallic = graph.addPass("debug_metallic");

    debugMetallic.addColourAttachment("backbuffer");
    debugMetallic.addDepthAttachment("depth");

    debugMetallic.addStorageBufferRead("global");
    debugMetallic.addStorageBufferRead("drawCommands");
    debugMetallic.addStorageBufferRead("materialCounts");
    debugMetallic.addStorageBufferRead("transforms");
    debugMetallic.addStorageBufferRead("meshData");

    debugMetallic.setExecuteFunction([&](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({});
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::METALLIC);
            if (!program)
                continue;
            cmd.bindProgram(program);
            cmd.bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}

void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugUnlit = graph.addPass("debug_unlit");

    debugUnlit.addColourAttachment("backbuffer");
    debugUnlit.addDepthAttachment("depth");

    debugUnlit.addStorageBufferRead("global");
    debugUnlit.addStorageBufferRead("drawCommands");
    debugUnlit.addStorageBufferRead("materialCounts");
    debugUnlit.addStorageBufferRead("transforms");
    debugUnlit.addStorageBufferRead("meshData");

    debugUnlit.setExecuteFunction([&](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({});
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            auto program = engine.getMaterial(material)->getVariant(cala::Material::Variant::UNLIT);
            if (!program)
                continue;
            cmd.bindProgram(program);
            cmd.bindBuffer(2, 0, engine.getMaterial(material)->buffer(), true);
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}

void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    auto& debugWorldPos = graph.addPass("debug_worldPos");

    debugWorldPos.addColourAttachment("backbuffer");
    debugWorldPos.addDepthAttachment("depth");

    debugWorldPos.addStorageBufferRead("global");
    debugWorldPos.addStorageBufferRead("drawCommands");
    debugWorldPos.addStorageBufferRead("materialCounts");
    debugWorldPos.addStorageBufferRead("transforms");
    debugWorldPos.addStorageBufferRead("meshData");

    debugWorldPos.setExecuteFunction([&](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({});
//            cmd.bindPipeline();
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd.bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_WORLDPOS));
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}

void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings, const char* backbuffer) {
    auto& debugWireframe = graph.addPass("debug_wireframe");

    debugWireframe.addColourAttachment(backbuffer);
    debugWireframe.addDepthAttachment("depth");

    debugWireframe.addStorageBufferRead("global");
    debugWireframe.addStorageBufferRead("drawCommands");
    debugWireframe.addStorageBufferRead("materialCounts");
    debugWireframe.addStorageBufferRead("transforms");
    debugWireframe.addStorageBufferRead("meshData");

    debugWireframe.setExecuteFunction([settings, &engine, &scene](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({
            .polygonMode = cala::backend::PolygonMode::LINE,
            .lineWidth = settings.wireframeThickness
        });
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd.bindProgram(engine.getProgram(cala::Engine::ProgramType::SOLID_COLOUR));
            cmd.pushConstants(cala::backend::ShaderStage::FRAGMENT, { &settings.wireframeColour, sizeof(settings.wireframeColour) });
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}

void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings, const char* backbuffer) {
    auto& debugNormalLines = graph.addPass("debug_normal_lines");

    debugNormalLines.addColourAttachment(backbuffer);
    debugNormalLines.addDepthAttachment("depth");

    debugNormalLines.addStorageBufferRead("global");
    debugNormalLines.addStorageBufferRead("drawCommands");
    debugNormalLines.addStorageBufferRead("materialCounts");
    debugNormalLines.addStorageBufferRead("transforms");
    debugNormalLines.addStorageBufferRead("meshData");

    debugNormalLines.setExecuteFunction([settings, &engine, &scene](cala::backend::vulkan::CommandBuffer& cmd, cala::RenderGraph& graph) {
        auto global = graph.getResource<cala::BufferResource>("global");
        auto drawCommands = graph.getResource<cala::BufferResource>("drawCommands");
        auto materialCounts = graph.getResource<cala::BufferResource>("materialCounts");
        cmd.clearDescriptors();
        cmd.bindBuffer(1, 0, global->handle);
        auto& renderable = scene._renderables[0].second.first;

        cmd.bindBindings(renderable.bindings);
        cmd.bindAttributes(renderable.attributes);
        cmd.bindDepthState({ true, true, cala::backend::CompareOp::LESS });
        cmd.bindRasterState({});
        cmd.bindVertexBuffer(0, engine.vertexBuffer());
        cmd.bindIndexBuffer(engine.indexBuffer());
        for (u32 material = 0; material < scene._materialCounts.size(); material++) {
            cmd.bindProgram(engine.getProgram(cala::Engine::ProgramType::DEBUG_NORMALS));
            cmd.pushConstants(cala::backend::ShaderStage::FRAGMENT, { &settings.wireframeColour, sizeof(settings.wireframeColour) });
            cmd.pushConstants(cala::backend::ShaderStage::GEOMETRY, { &settings.normalLength, sizeof(settings.normalLength) }, sizeof(settings.wireframeColour));
            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.drawIndirectCount(drawCommands->handle, scene._materialCounts[material].offset * sizeof(VkDrawIndexedIndirectCommand), materialCounts->handle, material * (sizeof(u32) * 2), scene._materialCounts[material].count);
        }
    });
}