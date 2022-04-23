#ifndef CALA_COMMANDBUFFERLIST_H
#define CALA_COMMANDBUFFERLIST_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
#include <Cala/backend/vulkan/CommandBuffer.h>

namespace cala::backend::vulkan {

    class Driver;

    class CommandBufferList {
    public:

        CommandBufferList(Driver& driver, QueueType queue);

        ~CommandBufferList();


        CommandBuffer* get();

        bool flush();

        u32 count() const { return _buffers.size(); }

    private:

        Driver& _driver;
        VkCommandPool _pool;
        VkQueue _queue;
        CommandBuffer* _current;
        ende::Vector<CommandBuffer> _buffers;

    };

}

#endif //CALA_COMMANDBUFFERLIST_H
