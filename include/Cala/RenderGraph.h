#ifndef CALA_RENDERGRAPH_H
#define CALA_RENDERGRAPH_H

#include <Cala/Engine.h>
#include <functional>
#include <tsl/robin_map.h>
#include <Cala/vulkan/Timer.h>
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
        u32 mipLevels = 1;
        vk::Format format = vk::Format::RGBA8_UNORM;
        vk::ImageUsage usage = vk::ImageUsage::SAMPLED | vk::ImageUsage::TRANSFER_SRC;
    };

    struct BufferResource : public Resource {
        u32 size = 0;
        vk::BufferUsage usage = vk::BufferUsage::STORAGE;
    };


    enum class ImageIndex : u32 {};
    enum class BufferIndex : u32 {};

    class RenderGraph;
    class RenderPass {
    public:

        enum class Type {
            GRAPHICS,
            COMPUTE,
            TRANSFER
        };

        RenderPass(RenderGraph* graph, const char* label);

        void setExecuteFunction(std::function<void(vk::CommandHandle, RenderGraph&)> func);

        void setDebugColour(const std::array<f32, 4>& colour);

        void setDebugGroup(const char* group) { _debugGroup = group; }

        void setDimensions(u32 width, u32 height);


        void addColourWrite(const char* label);
        void addColourWrite(ImageIndex index);

        void addColourRead(const char* label);
        void addColourRead(ImageIndex index);

        void addDepthWrite(const char* label);
        void addDepthWrite(ImageIndex index);

        void addDepthRead(const char* label);
        void addDepthRead(ImageIndex index);

        void addIndirectRead(const char* label);
        void addIndirectRead(BufferIndex index);

        void addVertexRead(const char* label);
        void addVertexRead(BufferIndex index);

        void addIndexRead(const char* label);
        void addIndexRead(BufferIndex index);

        void addStorageImageWrite(const char* label, vk::PipelineStage stage);
        void addStorageImageWrite(ImageIndex index, vk::PipelineStage stage);

        void addStorageImageRead(const char* label, vk::PipelineStage stage);
        void addStorageImageRead(ImageIndex index, vk::PipelineStage stage);

        void addStorageBufferWrite(const char* label, vk::PipelineStage stage);
        void addStorageBufferWrite(BufferIndex index, vk::PipelineStage stage);

        void addStorageBufferRead(const char* label, vk::PipelineStage stage);
        void addStorageBufferRead(BufferIndex index, vk::PipelineStage stage);

        void addUniformBufferRead(const char* label, vk::PipelineStage stage);
        void addUniformBufferRead(BufferIndex index, vk::PipelineStage stage);

        void addSampledImageRead(const char* label, vk::PipelineStage stage);
        void addSampledImageRead(ImageIndex index, vk::PipelineStage stage);

        void addBlitWrite(const char* label);
        void addBlitWrite(ImageIndex index);

        void addBlitRead(const char* label);
        void addBlitRead(ImageIndex index);

        void addTransferWrite(const char* label);
        void addTransferWrite(ImageIndex index);
        void addTransferWrite(BufferIndex index);

        void addTransferRead(const char* label);
        void addTransferRead(ImageIndex index);
        void addTransferRead(BufferIndex index);

//    private:

        Resource* reads(const char* label, vk::Access access, vk::PipelineStage stage, vk::ImageLayout layout);
        Resource* reads(ImageIndex index, vk::Access access, vk::PipelineStage stage, vk::ImageLayout layout);
        Resource* reads(BufferIndex index, vk::Access access, vk::PipelineStage stage, vk::ImageLayout layout);

        Resource* writes(const char* label, vk::Access access, vk::PipelineStage stage, vk::ImageLayout layout);
        Resource* writes(ImageIndex index, vk::Access access, vk::PipelineStage stage, vk::ImageLayout layout);
        Resource* writes(BufferIndex index, vk::Access access, vk::PipelineStage stage, vk::ImageLayout layout);


        friend RenderGraph;

        RenderGraph* _graph;

        const char* _label;

        std::function<void(vk::CommandHandle, RenderGraph&)> _function;

        Type _type;

        std::array<f32, 4> _debugColour;

        struct ResourceAccess {
            const char* label;
            i32 index;
            vk::Access access;
            vk::PipelineStage stage;
            vk::ImageLayout layout;
        };

        std::vector<ResourceAccess> _inputs;
        std::vector<ResourceAccess> _outputs;

        std::vector<u32> _colourAttachments;
        i32 _depthResource;
        u32 _width;
        u32 _height;

        vk::Framebuffer* _framebuffer;

        struct Barrier {
            const char* label = nullptr;
            i32 index = -1;
            vk::PipelineStage srcStage = vk::PipelineStage::TOP;
            vk::PipelineStage dstStage = vk::PipelineStage::BOTTOM;
            vk::Access srcAccess = vk::Access::NONE;
            vk::Access dstAccess = vk::Access::NONE;
            vk::ImageLayout srcLayout = vk::ImageLayout::UNDEFINED;
            vk::ImageLayout dstLayout = vk::ImageLayout::UNDEFINED;
        };
        std::vector<Barrier> _barriers;

        const char* _debugGroup = nullptr;

    };

    class RenderGraph {
    public:

        RenderGraph(Engine* engine);

        RenderPass& addPass(const char* label, RenderPass::Type type = RenderPass::Type::GRAPHICS);

        void setBackbuffer(const char* label);

        void setBackbufferDimensions(u32 width, u32 height);

        vk::ImageHandle getBackbuffer() { return getImage(_backbuffer); }

        ende::math::Vec<2, u32> getBackbufferDimensions() { return { _backbufferWidth, _backbufferHeight }; }

        ImageIndex addImageResource(const char* label, ImageResource resource, vk::ImageHandle handle = {});

        BufferIndex addBufferResource(const char* label, BufferResource resource, vk::BufferHandle handle = {});

        u32 addAlias(const char* label, const char* alias);
        u32 addAlias(u32 resourceIndex, const char* alias);


        ImageResource* getImageResource(const char* label);
        ImageResource* getImageResource(ImageIndex resourceIndex);

        BufferResource* getBufferResource(const char* label);
        BufferResource* getBufferResource(BufferIndex resourceIndex);

        vk::ImageHandle getImage(const char* label);
        vk::ImageHandle getImage(ImageIndex resourceIndex);

        vk::BufferHandle getBuffer(const char* label);
        vk::BufferHandle getBuffer(BufferIndex resourceIndex);


        bool compile();

        bool execute(vk::CommandHandle  cmd);

        void reset();

        std::span<std::pair<const char*, vk::Timer>> getTimers() {
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
        u32 _backbufferWidth;
        u32 _backbufferHeight;

        std::vector<RenderPass> _passes;

        tsl::robin_map<const char*, u32> _labelToIndex;

        std::vector<std::unique_ptr<Resource>> _resources;
        std::vector<std::vector<const char*>> _aliases;
        std::vector<vk::ImageHandle> _images;
        std::vector<vk::BufferHandle> _buffers;

        std::vector<std::pair<const char*, vk::Timer>> _timers[vk::FRAMES_IN_FLIGHT];

        std::vector<RenderPass*> _orderedPasses;

    };

}

#endif //CALA_RENDERGRAPH_H
