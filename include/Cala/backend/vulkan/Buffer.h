#ifndef CALA_BUFFER_H
#define CALA_BUFFER_H

#include <vulkan/vulkan.h>
#include <Cala/backend/primitives.h>
#include <Ende/Span.h>
#include <Cala/backend/vulkan/vk_mem_alloc.h>

namespace cala::backend::vulkan {

    class Device;

    class Buffer {
    public:

        Buffer(Device& driver, u32 size, BufferUsage usage, MemoryProperties flags = MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);

        ~Buffer();

        Buffer(const Buffer&) = delete;
        Buffer(Buffer&& rhs) noexcept;

        Buffer& operator=(const Buffer&) = delete;
        Buffer& operator=(Buffer&& rhs) noexcept;


        struct Mapped {
            void* address = nullptr;
            Buffer* buffer = nullptr;
            ~Mapped();

            Mapped();

            Mapped(const Mapped&) = delete;

            Mapped& operator=(const Mapped&) = delete;

            Mapped(Mapped&& rhs) noexcept;

            Mapped& operator=(Mapped&& rhs) noexcept;
        };
        Mapped map(u32 offset = 0, u32 size = 0);

        void unmap();

        void data(ende::Span<const void> data, u32 offset = 0);

        void resize(u32 capacity);


        class View {
        public:
            View();

            View(Buffer& buffer);

            View(Buffer& buffer, u32 size, u32 offset);

            Mapped map(u32 offset = 0, u32 size = 0);

            void data(ende::Span<const void> data, u32 offset = 0);

            u32 size() const { return _size; }

            u32 offset() const { return _offset; }

            Buffer& buffer() const { return *_parent; }

            explicit operator bool() const { return _parent != nullptr; }

        private:
            Buffer* _parent;
            u32 _size;
            u32 _offset;

        };


        VkBuffer buffer() const { return _buffer; }

        u32 size() const { return _size; }

        BufferUsage usage() const { return _usage; }

    private:

        Device& _driver;
        VkBuffer _buffer;
        VmaAllocation _allocation;
        u32 _size;
        BufferUsage _usage;
        MemoryProperties _flags;

    };

}

#endif //CALA_BUFFER_H
