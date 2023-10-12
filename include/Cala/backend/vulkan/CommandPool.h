#ifndef CALA_COMMANDPOOL_H
#define CALA_COMMANDPOOL_H

#include <Cala/backend/vulkan/CommandBuffer.h>

namespace cala::backend::vulkan {

    class CommandPool {
    public:

        CommandPool(Device* device, QueueType queueType);

        ~CommandPool();

        CommandPool(const CommandPool&) = delete;

        CommandPool& operator=(const CommandPool&)  = delete;

        void reset();

        CommandBuffer& getBuffer();

        void destroy();

    private:

        Device* _device;
        VkCommandPool _pool;
        std::vector<CommandBuffer> _buffers;
        QueueType _queueType;
        u32 _index;

    };

}

#endif //CALA_COMMANDPOOL_H
