#ifndef CALA_DEBUGPASSES_H
#define CALA_DEBUGPASSES_H

#include <Cala/RenderGraph.h>
#include <Cala/Scene.h>
#include <Cala/Renderer.h>

struct ClusterDebugInput {
    cala::ImageIndex backbufferDebug;
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::BufferIndex global;
    cala::BufferIndex lightGrid;
};
void debugClusters(cala::RenderGraph& graph, cala::Engine& engine, cala::vk::Swapchain& swapchain, ClusterDebugInput input);

struct NormalDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
    cala::BufferIndex materialCount;
    cala::ImageIndex pixelPositions;
    cala::BufferIndex dispatchBuffer;
    cala::BufferIndex camera;
    cala::BufferIndex meshData;
    cala::BufferIndex transforms;
    cala::BufferIndex vertexBuffer;
    cala::BufferIndex indexBuffer;
};
void debugNormalPass(cala::RenderGraph& graph, cala::Engine& engine, NormalDebugInput input);

struct RoughnessDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
    cala::BufferIndex materialCount;
    cala::ImageIndex pixelPositions;
    cala::BufferIndex dispatchBuffer;
    cala::BufferIndex camera;
    cala::BufferIndex meshData;
    cala::BufferIndex transforms;
    cala::BufferIndex vertexBuffer;
    cala::BufferIndex indexBuffer;
};
void debugRoughnessPass(cala::RenderGraph& graph, cala::Engine& engine, RoughnessDebugInput input);

struct MetallicDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
    cala::BufferIndex materialCount;
    cala::ImageIndex pixelPositions;
    cala::BufferIndex dispatchBuffer;
    cala::BufferIndex camera;
    cala::BufferIndex meshData;
    cala::BufferIndex transforms;
    cala::BufferIndex vertexBuffer;
    cala::BufferIndex indexBuffer;
};
void debugMetallicPass(cala::RenderGraph& graph, cala::Engine& engine, MetallicDebugInput input);

struct UnlitDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
    cala::BufferIndex materialCount;
    cala::ImageIndex pixelPositions;
    cala::BufferIndex dispatchBuffer;
    cala::BufferIndex camera;
    cala::BufferIndex meshData;
    cala::BufferIndex transforms;
    cala::BufferIndex vertexBuffer;
    cala::BufferIndex indexBuffer;
};
void debugUnlitPass(cala::RenderGraph& graph, cala::Engine& engine, UnlitDebugInput input);

struct WorldPosDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
    cala::BufferIndex materialCount;
    cala::ImageIndex pixelPositions;
    cala::BufferIndex dispatchBuffer;
    cala::BufferIndex camera;
    cala::BufferIndex meshData;
    cala::BufferIndex transforms;
    cala::BufferIndex vertexBuffer;
    cala::BufferIndex indexBuffer;
};
void debugWorldPositionPass(cala::RenderGraph& graph, cala::Engine& engine, WorldPosDebugInput input);

struct WireframeDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
    cala::BufferIndex camera;
    cala::BufferIndex meshData;
    cala::BufferIndex transforms;
    cala::BufferIndex vertexBuffer;
    cala::BufferIndex indexBuffer;
};
void debugWireframePass(cala::RenderGraph& graph, cala::Engine& engine, cala::Renderer::Settings settings, WireframeDebugInput input);

//void debugNormalLinesPass(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings);

struct FrustumDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::BufferIndex global;
    cala::BufferIndex camera;
};
void debugFrustum(cala::RenderGraph& graph, cala::Engine& engine, cala::Renderer::Settings settings, FrustumDebugInput input);

struct DepthDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex depth;
    cala::BufferIndex global;
};
void debugDepthPass(cala::RenderGraph& graph, cala::Engine& engine, DepthDebugInput input);

struct MeshletDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
};
void debugMeshletPass(cala::RenderGraph& graph, cala::Engine& engine, MeshletDebugInput input);

struct PrimitiveDebugInput {
    cala::ImageIndex backbuffer;
    cala::ImageIndex visbility;
    cala::BufferIndex global;
};
void debugPrimitivePass(cala::RenderGraph& graph, cala::Engine& engine, PrimitiveDebugInput input);

void debugClusterFrustums(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene, cala::Renderer::Settings settings);

void debugVxgi(cala::RenderGraph& graph, cala::Engine& engine);

#endif //CALA_DEBUGPASSES_H
