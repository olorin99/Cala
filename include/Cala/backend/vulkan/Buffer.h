#ifndef CALA_BUFFER_H
#define CALA_BUFFER_H

#include <vulkan/vulkan.h>
#include <Cala/backend/primitives.h>
#include <Ende/Span.h>
#include <Cala/backend/vulkan/vk_mem_alloc.h>

namespace cala::backend::vulkan {

    class Driver;

    class Buffer {
    public:

        Buffer(Driver& driver, u32 size, BufferUsage usage, MemoryProperties flags = MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);

        ~Buffer();

        Buffer(const Buffer&) = delete;
        Buffer(Buffer&& rhs);


        struct Mapped {
            void* address = nullptr;
            Buffer* buffer = nullptr;
            ~Mapped();
        };
        Mapped map(u32 offset, u32 size);

        void unmap();

        void data(ende::Span<const void> data, u32 offset = 0);


        VkBuffer buffer() const { return _buffer; }

        u32 size() const { return _size; }

        BufferUsage usage() const { return _usage; }

    private:

        Driver& _driver;
        VkBuffer _buffer;
        VmaAllocation _allocation;
        u32 _size;
        BufferUsage _usage;
        MemoryProperties _flags;

    };

}

#endif //CALA_BUFFER_H
