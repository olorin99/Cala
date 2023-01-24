#ifndef CALA_RENDERGRAPH_H
#define CALA_RENDERGRAPH_H

#include <Cala/Engine.h>
#include <functional>
#include "../third_party/tsl/robin_map.h"

namespace cala {

    class RenderGraph;

    struct AttachmentInfo {
        u32 width = 1;
        u32 height = 1;
        backend::Format format = backend::Format::RGBA8_SRGB;
        bool persistent = true;
        bool matchSwapchain = true;
        bool clear = true;
        ImageHandle handle;
    };



    class RenderPass {
    public:

        void addColourOutput(const char* label, AttachmentInfo info);

        void addColourOutput(const char* label);

        void setDepthOutput(const char* label, AttachmentInfo info);

        void addAttachmentInput(const char* label);

        void setDepthInput(const char* label);

        void setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&)> func);

        ~RenderPass();
    private:

        friend RenderGraph;

        RenderPass(RenderGraph* graph, const char* name);

        RenderGraph* _graph;
        const char* _passName;

        ende::Vector<const char*> _inputs;
        ende::Vector<const char*> _outputs;
        const char* _depthAttachment;

        std::function<void(backend::vulkan::CommandBuffer&)> _executeFunc;

        cala::backend::vulkan::RenderPass* _renderPass;
        cala::backend::vulkan::Framebuffer* _framebuffer;

    };

    class RenderGraph {
    public:

        RenderGraph(Engine* engine);

        RenderPass& addPass(const char* name);

        void setBackbuffer(const char* label);

        bool compile();

        bool execute(backend::vulkan::CommandBuffer& cmd, u32 index = 0);

        void reset();

    private:
        friend RenderPass;

        Engine* _engine;

        ende::Vector<RenderPass> _passes;

        tsl::robin_map<const char*, AttachmentInfo> _attachments;

        const char* _backbuffer;

        ende::Vector<RenderPass*> _orderedPasses;

    };

}

#endif //CALA_RENDERGRAPH_H
