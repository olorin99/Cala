#ifndef CALA_COMMANDBUFFERLIST_H
#define CALA_COMMANDBUFFERLIST_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>

namespace cala::backend::vulkan {

    class CommandBufferList {
    public:

        struct CommandBuffer {
            VkCommandBuffer buffer;
            bool free;
            VkSemaphore signal;
        };

        CommandBufferList(VkDevice device, u32 queueIndex);

        ~CommandBufferList();


        VkCommandBuffer get();

        bool flush();

        void waitSemaphore(VkSemaphore wait);

        VkSemaphore signal();

    private:

        VkDevice _device;
        VkCommandPool _pool;
        CommandBuffer* _current;
        ende::Vector<CommandBuffer> _buffers;
        VkQueue _queue;
        VkSemaphore _wait;
        VkSemaphore _renderFinish;

    };

}

#endif //CALA_COMMANDBUFFERLIST_H
