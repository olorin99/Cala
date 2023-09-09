#ifndef CALA_BUFFER_H
#define CALA_BUFFER_H

#include <Cala/backend/primitives.h>
#include <Ende/Span.h>
#include <volk.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "../../third_party/vk_mem_alloc.h"

namespace cala::backend::vulkan {

    class Device;

    class Buffer {
    public:

        Buffer(Device* device);

        Buffer() = delete;

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

        struct Barrier {
            Buffer* buffer;
            Access srcAccess;
            Access dstAccess;
        };

        Barrier barrier(Access dstAccess);

        void invalidate() { _invalidated = true; }

        void validate() { _invalidated = false; }

        bool invalidated() const { return _invalidated; }


        VkBuffer buffer() const { return _buffer; }

        u32 size() const { return _size; }

        BufferUsage usage() const { return _usage; }

        MemoryProperties flags() const { return _flags; }

        bool persistentlyMapped() const { return _mapped.address != nullptr; }

        void* persistentMapping() { return _mapped.address; }

    private:
        friend Device;

        Device* _device;
        VkBuffer _buffer;
        VmaAllocation _allocation;
        u32 _size;
        BufferUsage _usage;
        MemoryProperties _flags;
        Mapped _mapped;
        bool _invalidated;

    };

}

#endif //CALA_BUFFER_H
