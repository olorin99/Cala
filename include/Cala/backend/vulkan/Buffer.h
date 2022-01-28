#ifndef CALA_BUFFER_H
#define CALA_BUFFER_H

#include <vulkan/vulkan.h>
#include <Cala/backend/vulkan/Context.h>

namespace cala::backend::vulkan {

    class Buffer {
    public:

        Buffer(Context& context, u32 size, u32 usage, u32 flags);

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

    private:

        Context& _context;
        VkBuffer _buffer;
        VkDeviceMemory _memory;
        u32 _size;
        u32 _usage;
        u32 _flags;

    };

}

#endif //CALA_BUFFER_H
