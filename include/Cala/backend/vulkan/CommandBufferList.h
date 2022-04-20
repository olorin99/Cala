#ifndef CALA_COMMANDBUFFERLIST_H
#define CALA_COMMANDBUFFERLIST_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/Context.h>

namespace cala::backend::vulkan {

    class CommandBufferList {
    public:

        CommandBufferList(const Context& context, u32 queueIndex);

        ~CommandBufferList();


        CommandBuffer* get();

        bool flush();

        u32 count() const { return _buffers.size(); }

    private:

        const Context& _context;
        VkCommandPool _pool;
        VkQueue _queue;
        CommandBuffer* _current;
        ende::Vector<CommandBuffer> _buffers;

    };

}

#endif //CALA_COMMANDBUFFERLIST_H
