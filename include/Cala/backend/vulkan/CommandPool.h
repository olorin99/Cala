#ifndef CALA_COMMANDPOOL_H
#define CALA_COMMANDPOOL_H

#include <Cala/backend/vulkan/CommandBuffer.h>

namespace cala::backend::vulkan {

    class CommandPool {
    public:

        CommandPool(Device* device, QueueType queueType);

        ~CommandPool();

        void reset();

        CommandBuffer& getBuffer();

    private:

        Device* _device;
        VkCommandPool _pool;
        ende::Vector<CommandBuffer> _buffers;
        QueueType _queueType;
        u32 _index;

    };

}

#endif //CALA_COMMANDPOOL_H
