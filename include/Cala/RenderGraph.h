#ifndef CALA_RENDERGRAPH_H
#define CALA_RENDERGRAPH_H

#include <Cala/Engine.h>
#include <functional>
#include "../third_party/tsl/robin_map.h"
#include <Cala/backend/vulkan/Timer.h>
#include <Ende/math/Vec.h>

namespace cala {

    struct Resource {
        virtual ~Resource() = default;

        u32 index = 0;
        const char* label = nullptr;
    };

    struct ImageResource : public Resource {
        bool matchSwapchain = true;
        u32 width = 1;
        u32 height = 1;
        u32 depth = 1;
        backend::Format format = backend::Format::RGBA8_UNORM;
        backend::ImageUsage usage = backend::ImageUsage::TRANSFER_SRC;
    };

    struct BufferResource : public Resource {
        u32 size = 0;
        backend::BufferUsage usage = backend::BufferUsage::STORAGE;
    };

    class RenderGraph;
    class RenderPass {
    public:

        RenderPass(RenderGraph* graph, const char* label);

        void setExecuteFunction(std::function<void(backend::vulkan::CommandHandle, RenderGraph&)> func);

        void setDebugColour(const std::array<f32, 4>& colour);


        void addColourWrite(const char* label);

        void addColourRead(const char* label);

        void addDepthWrite(const char* label);

        void addDepthRead(const char* label);

        void addIndirectRead(const char* label);

        void addVertexRead(const char* label);

        void addIndexRead(const char* label);

        void addStorageImageWrite(const char* label, backend::PipelineStage stage);

        void addStorageImageRead(const char* label, backend::PipelineStage stage);

        void addStorageBufferWrite(const char* label, backend::PipelineStage stage);

        void addStorageBufferRead(const char* label, backend::PipelineStage stage);

        void addSampledImageRead(const char* label, backend::PipelineStage stage);

//    private:

        Resource* reads(const char* label, backend::Access access, backend::PipelineStage stage, backend::ImageLayout layout);

        Resource* writes(const char* label, backend::Access access, backend::PipelineStage stage, backend::ImageLayout layout);


        friend RenderGraph;

        RenderGraph* _graph;

        const char* _label;

        std::function<void(backend::vulkan::CommandHandle, RenderGraph&)> _function;

        bool _compute;

        std::array<f32, 4> _debugColour;

        struct ResourceAccess {
            const char* label;
            i32 index;
            backend::Access access;
            backend::PipelineStage stage;
            backend::ImageLayout layout;
        };

        std::vector<ResourceAccess> _inputs;
        std::vector<ResourceAccess> _outputs;

        std::vector<u32> _colourAttachments;
        i32 _depthResource;

        backend::vulkan::Framebuffer* _framebuffer;

        struct Barrier {
            const char* label;
            i32 index = -1;
            backend::PipelineStage srcStage = backend::PipelineStage::TOP;
            backend::PipelineStage dstStage = backend::PipelineStage::BOTTOM;
            backend::Access srcAccess = backend::Access::NONE;
            backend::Access dstAccess = backend::Access::NONE;
            backend::ImageLayout srcLayout = backend::ImageLayout::UNDEFINED;
            backend::ImageLayout dstLayout = backend::ImageLayout::UNDEFINED;
        };
        std::vector<Barrier> _barriers;

    };

    class RenderGraph {
    public:

        RenderGraph(Engine* engine);


        RenderPass& addPass(const char* label, bool compute = false);

        void setBackbuffer(const char* label);


        void addImageResource(const char* label, ImageResource resource, backend::vulkan::ImageHandle handle = {});

        void addBufferResource(const char* label, BufferResource resource, backend::vulkan::BufferHandle handle = {});

        void addAlias(const char* label, const char* alias);


        ImageResource* getImageResource(const char* label);

        BufferResource* getBufferResource(const char* label);

        backend::vulkan::ImageHandle getImage(const char* label);

        backend::vulkan::BufferHandle getBuffer(const char* label);


        bool compile(backend::vulkan::Swapchain* swapchain);

        bool execute(backend::vulkan::CommandHandle  cmd, u32 index = 0);

        void reset();

        std::span<std::pair<const char*, backend::vulkan::Timer>> getTimers() {
            u32 frameIndex = _engine->device().frameIndex();
            assert(_orderedPasses.size() <= _timers[frameIndex].size());
            return { _timers[frameIndex].data(), static_cast<u32>(_orderedPasses.size()) };
        }

//    private:

        friend RenderPass;

        void buildResources();

        void buildBarriers();

        void buildRenderPasses();

        void log();

        Engine* _engine;

        const char* _backbuffer;

        std::vector<RenderPass> _passes;

        tsl::robin_map<const char*, u32> _labelToIndex;

        std::vector<std::unique_ptr<Resource>> _resources;
        std::vector<std::vector<const char*>> _aliases;
        std::vector<backend::vulkan::ImageHandle> _images;
        std::vector<backend::vulkan::BufferHandle> _buffers;

        std::vector<std::pair<const char*, backend::vulkan::Timer>> _timers[backend::vulkan::FRAMES_IN_FLIGHT];

        std::vector<RenderPass*> _orderedPasses;

        backend::vulkan::Swapchain* _swapchain;
    };

}

#endif //CALA_RENDERGRAPH_H
