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

        virtual void devirtualize(Engine* engine, backend::vulkan::Swapchain* swapchain) = 0;

        virtual void destroyResource(Engine* engine) = 0;

        virtual bool operator==(Resource* rhs) = 0;

        virtual bool operator!=(Resource* rhs) = 0;
    };

    struct ImageResource : public Resource {

        u32 width = 1;
        u32 height = 1;
        backend::Format format = backend::Format::RGBA8_UNORM;
        bool matchSwapchain = true;
        bool clear = true;
        backend::ImageUsage usage = backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST;
        backend::vulkan::ImageHandle handle;

        void devirtualize(Engine* engine, backend::vulkan::Swapchain* swapchain) override;

        void destroyResource(Engine* engine) override;

        bool operator==(Resource* rhs) override;

        bool operator!=(Resource* rhs) override;
    };

    struct BufferResource : public Resource {

        u32 size = 1;
        backend::BufferUsage usage = backend::BufferUsage::UNIFORM;

        backend::vulkan::BufferHandle handle;

        void devirtualize(Engine* engine, backend::vulkan::Swapchain* swapchain) override;

        void destroyResource(Engine* engine) override;

        bool operator==(Resource* rhs) override;

        bool operator!=(Resource* rhs) override;
    };

    class RenderPass {
    public:

    private:
        bool reads(const char* label, bool storage = false);

        bool writes(const char* label);
    public:

        void addColourAttachment(const char* label);

        void addDepthAttachment(const char* label);

        void addDepthReadAttachment(const char* label);

        void addStorageImageRead(const char* label);

        void addStorageImageWrite(const char* label);

        void addStorageBufferRead(const char* label);

        void addStorageBufferWrite(const char* label);

        void addSampledImageRead(const char* label);

        void addSampledImageWrite(const char* label);



        void setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&, RenderGraph&)> func);

        void setDebugColour(std::array<f32, 4> colour);

        ~RenderPass();
    private:

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

        bool compile(backend::vulkan::Swapchain* swapchain);

        bool execute(backend::vulkan::CommandBuffer& cmd, u32 index = 0);

        void reset();

        ende::Span<std::pair<const char*, backend::vulkan::Timer>> getTimers() {
            u32 offIndex = _engine->device().frameIndex();
            assert(_orderedPasses.size() <= _timers[offIndex].size());
            return { _timers[offIndex].data(), static_cast<u32>(_orderedPasses.size()) };
        }

        template<class T>
        T* getResource(const char* label) {
            auto it = _attachmentMap.find(label);
            if (it == _attachmentMap.end())
                return nullptr;

            u32 index = it.value().index;
            auto* t = it.value().internal ? _internalResources[index].get() : _externalResources[index].get();
            return dynamic_cast<T*>(t);
        }

        template <typename T>
        void addResource(const char* label, T resource, bool internal = true) {
            auto it = _attachmentMap.find(label);
            if (_attachmentMap.end() == it) {
                if (internal) {
                    _internalResources.push(std::make_unique<T>(std::move(resource)));
                    _attachmentMap.emplace(label, ResourcePointer{ (u32)_internalResources.size() - 1, internal });
                } else {
                    _externalResources.push(std::make_unique<T>(std::move(resource)));
                    _attachmentMap.emplace(label, ResourcePointer{ (u32)_externalResources.size() - 1, internal });
                }
            } else {
                assert(it.value().internal == internal);
                u32 index = it.value().index;

                if (internal) {
                    assert(index < _internalResources.size());
                    if (*_internalResources[index] != &resource) {
                        _internalResources[index]->destroyResource(_engine);
                        _internalResources[index] = std::make_unique<T>(std::move(resource));
                    }
                } else {
                    assert(index < _externalResources.size());
                    if (*_externalResources[index] != &resource) {
                        _externalResources[index]->destroyResource(_engine);
                        _externalResources[index] = std::make_unique<T>(std::move(resource));
                    }
                }
            }
        }

    private:
        friend RenderPass;

        Engine* _engine;
        backend::vulkan::Swapchain* _swapchain;

        ende::Vector<RenderPass> _passes;
        ende::Vector<std::pair<const char*, backend::vulkan::Timer>> _timers[2];

        struct ResourcePointer {
            u32 index;
            bool internal;
        };

        tsl::robin_map<const char*, ResourcePointer> _attachmentMap;

        ende::Vector<std::unique_ptr<Resource>> _internalResources;
        ende::Vector<std::unique_ptr<Resource>> _externalResources;

        const char* _backbuffer;

        ende::Vector<RenderPass*> _orderedPasses;

    };

}

#endif //CALA_RENDERGRAPH_H
