#ifndef CALA_BUFFER_H
#define CALA_BUFFER_H

#include <Cala/vulkan/primitives.h>
#include <span>
#include "volk/volk.h"
#include <string>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

namespace cala::vk {

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

        void _data(u8, std::span<u8> data, u32 offset = 0);

        void _data(u8, std::span<const u8> data, u32 offset = 0);

        template<typename T>
        void data(std::span<T> data, u32 offset = 0) {
            _data(0, std::span<u8>(reinterpret_cast<u8*>(data.data()), data.size() * sizeof(T)), offset);
        }

        template<typename T>
        void data(std::span<const T> data, u32 offset = 0) {
            _data(0, std::span<const u8>(reinterpret_cast<const u8*>(data.data()), data.size() * sizeof(T)), offset);
        }


        class View {
        public:
            View();

            View(Buffer& buffer);

            View(Buffer& buffer, u32 size, u32 offset);

            Mapped map(u32 offset = 0, u32 size = 0);

            void data(std::span<const void> data, u32 offset = 0);

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
            PipelineStage srcStage;
            PipelineStage dstStage;
            Access srcAccess;
            Access dstAccess;
        };

        Barrier barrier(PipelineStage srcStage, PipelineStage dstStage, Access dstAccess);

        Barrier barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess);

        void invalidate() { _invalidated = true; }

        void validate() { _invalidated = false; }

        bool invalidated() const { return _invalidated; }


        VkBuffer buffer() const { return _buffer; }

        u64 address() const { return _address; }

        u32 size() const { return _size; }

        BufferUsage usage() const { return _usage; }

        MemoryProperties flags() const { return _flags; }

        bool persistentlyMapped() const { return _mapped.address != nullptr; }

        void* persistentMapping() { return _mapped.address; }

        const std::string& debugName() const { return _debugName; }

    private:
        friend Device;

        Device* _device;
        VkBuffer _buffer;
        VkDeviceAddress _address;
        VmaAllocation _allocation;
        u32 _size;
        BufferUsage _usage;
        MemoryProperties _flags;
        Mapped _mapped;
        bool _invalidated;
        std::string _debugName;

    };

}

#endif //CALA_BUFFER_H
