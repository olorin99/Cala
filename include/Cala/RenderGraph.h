#ifndef CALA_RENDERGRAPH_H
#define CALA_RENDERGRAPH_H

#include <Cala/Engine.h>
#include <functional>
#include "../third_party/tsl/robin_map.h"
#include <Cala/backend/vulkan/Timer.h>

namespace cala {

    class RenderGraph;

    struct Resource {
        bool transient = true;

        virtual ~Resource() = default;

        virtual void devirtualize(Engine* engine) = 0;
    };

    struct ImageResource : public Resource {

        u32 width = 1;
        u32 height = 1;
        backend::Format format = backend::Format::RGBA8_UNORM;
        bool matchSwapchain = true;
        bool clear = true;
        ImageHandle handle;

        void devirtualize(Engine* engine) override;

    };

    struct BufferResource : public Resource {

        u32 size = 1;
        backend::BufferUsage usage = backend::BufferUsage::UNIFORM;

        BufferHandle handle;

        void devirtualize(Engine* engine) override;
    };

    class RenderPass {
    public:

        void addColourOutput(const char* label, ImageResource info);

        void addColourOutput(const char* label);

        void setDepthOutput(const char* label, ImageResource info);

        void addImageInput(const char* label, bool storage = false);

        void addImageOutput(const char* label, bool storage = false);

        void addImageOutput(const char* label, ImageResource info, bool storage = false);

        void setDepthInput(const char* label);

        void addBufferInput(const char* label, BufferResource info);

        void addBufferOutput(const char* label, BufferResource info);

        void addBufferInput(const char* label);

        void addBufferOutput(const char* label);

        void setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&, RenderGraph&)> func);

        void setDebugColour(std::array<f32, 4> colour);

        ~RenderPass();
//    private:

        friend RenderGraph;

        RenderPass(RenderGraph* graph, const char* name, u32 index);

        RenderGraph* _graph;
        const char* _passName;

        ende::Vector<std::pair<const char*, bool>> _inputs;
        ende::Vector<const char*> _outputs;
        ende::Vector<const char*> _attachments;

        std::function<void(backend::vulkan::CommandBuffer&, RenderGraph&)> _executeFunc;

        std::array<f32, 4> _debugColour;
        u32 _passTimer;

        cala::backend::vulkan::Framebuffer* _framebuffer;

    };

    class RenderGraph {
    public:

        RenderGraph(Engine* engine);

        ~RenderGraph();

        RenderPass& addPass(const char* name);

        void setBackbuffer(const char* label);

        bool compile();

        bool execute(backend::vulkan::CommandBuffer& cmd, u32 index = 0);

        void reset();

        ende::Span<std::pair<const char*, backend::vulkan::Timer>> getTimers() {
            assert(_orderedPasses.size() <= _timers.size());
            return { _timers.data(), static_cast<u32>(_orderedPasses.size()) };
        }

        template<class T>
        T* getResource(const char* label) {
            auto it = _attachmentMap.find(label);
            if (it == _attachmentMap.end())
                return nullptr;
            return dynamic_cast<T*>(it.value());
        }

    private:
        friend RenderPass;

        Engine* _engine;

        ende::Vector<RenderPass> _passes;
        ende::Vector<std::pair<const char*, backend::vulkan::Timer>> _timers;

        tsl::robin_map<const char*, Resource*> _attachmentMap;

        const char* _backbuffer;

        ende::Vector<RenderPass*> _orderedPasses;

    };

}

#endif //CALA_RENDERGRAPH_H
