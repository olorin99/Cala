#ifndef CALA_DEBUGPASSES_H
#define CALA_DEBUGPASSES_H

#include <Cala/RenderGraph.h>
#include <Cala/Scene.h>
#include <Cala/Renderer.h>

void debugClusters(cala::RenderGraph& graph, cala::Engine& engine, cala::backend::vulkan::Swapchain& swapchain);

void debugNormalPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene);

void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene);

void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene);

void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene);

void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene);

void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings);

void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings);

void debugVxgi(cala::RenderGraph& graph, cala::Engine& engine, cala::Renderer::Settings settings);

#endif //CALA_DEBUGPASSES_H
