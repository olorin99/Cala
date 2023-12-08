#ifndef CALA_COMMANDPOOL_H
#define CALA_COMMANDPOOL_H

#include <Cala/vulkan/CommandBuffer.h>
#include <Cala/vulkan/Handle.h>

namespace cala::vk {

    class CommandPool {
    public:

        CommandPool() = default;

        CommandPool(Device* device, QueueType queueType);

        ~CommandPool();

        CommandPool(const CommandPool&) = delete;

        CommandPool& operator=(const CommandPool&)  = delete;

        CommandPool(CommandPool&& rhs) noexcept;

        CommandPool& operator=(CommandPool&& rhs) noexcept;

        void reset();

        CommandHandle getBuffer();

        void destroy();

    private:
        friend CommandHandle;

        Device* _device = nullptr;
        VkCommandPool _pool = VK_NULL_HANDLE;
        std::vector<CommandBuffer> _buffers = {};
        QueueType _queueType = QueueType::NONE;
        u32 _index = 0;

    };

}

#endif //CALA_COMMANDPOOL_H
