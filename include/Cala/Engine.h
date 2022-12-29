#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Vector.h>
#include <Ende/Shared.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>

#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Driver.h>

namespace cala {

    class Engine;
    template <typename T>
    class Handle {
    public:

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

    class Engine {
    public:

        Engine(backend::Platform& platform);

        backend::vulkan::Driver& driver() { return _driver; }



        BufferHandle createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags = backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);

        ImageHandle createImage(backend::vulkan::Image::CreateInfo info);

    private:
        friend BufferHandle;
        friend ImageHandle;

        backend::vulkan::Driver _driver;

        ende::Vector<backend::vulkan::Buffer> _buffers;
        ende::Vector<backend::vulkan::Image> _images;


    };

}

#endif //CALA_ENGINE_H
