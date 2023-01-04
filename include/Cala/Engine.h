#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Vector.h>
#include <vector>
#include <Ende/Shared.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>

#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Driver.h>

namespace cala {

    class Probe;

    class Engine;
    template <typename T>
    class Handle {
    public:

        Handle() = default;

        T& operator*() noexcept;

        T* operator->() noexcept;

    private:
        friend Engine;

        Handle(Engine* engine, u32 index)
                : _engine(engine),
                  _index(index)
        {}

        Engine* _engine;
        u32 _index;

    };
    using BufferHandle = Handle<backend::vulkan::Buffer>;
    using ImageHandle = Handle<backend::vulkan::Image>;
    using ProgramHandle = Handle<backend::vulkan::ShaderProgram>;

    class Renderer;

    class Engine {
    public:

        Engine(backend::Platform& platform, bool clear = true);

        ~Engine();

        backend::vulkan::Driver& driver() { return _driver; }



        BufferHandle createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags = backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);

        ImageHandle createImage(backend::vulkan::Image::CreateInfo info);

        ProgramHandle createProgram(backend::vulkan::ShaderProgram&& program);

    private:
        friend Renderer;
        friend BufferHandle;
        friend ImageHandle;
        friend ProgramHandle;

        backend::vulkan::Driver _driver;

        backend::vulkan::Sampler _defaultSampler;

        backend::vulkan::RenderPass _shadowPass;
        backend::vulkan::ShaderProgram* _pointShadowProgram;
        backend::vulkan::ShaderProgram* _directShadowProgram;

        ImageHandle _defaultPointShadow;
        backend::vulkan::Image::View _defaultPointShadowView;
        ImageHandle _defaultDirectionalShadow;
        backend::vulkan::Image::View _defaultDirectionalShadowView;

        ende::Vector<backend::vulkan::Buffer> _buffers;
        ende::Vector<backend::vulkan::Image> _images;
        ende::Vector<backend::vulkan::ShaderProgram> _programs;
        std::vector<Probe> _shadowProbes; //TODO: fix vectors in Ende, Optional implementation sucks

        Probe& getShadowProbe(u32 index);


    };

}

#endif //CALA_ENGINE_H
