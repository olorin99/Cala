#ifndef CALA_CONTEXT_H
#define CALA_CONTEXT_H

#include <vulkan/vulkan.h>

#include <Ende/Vector.h>
#include <Ende/Span.h>

#include <Cala/backend/vulkan/CommandBufferList.h>

namespace cala::backend::vulkan {

    class Context {
    public:

        Context(ende::Span<const char*> extensions);

        ~Context();

        u32 queueIndex(u32 flags);

        VkQueue getQueue(u32 flags);

        u32 memoryIndex(u32 filter, VkMemoryPropertyFlags properties);



        std::pair<VkBuffer, VkDeviceMemory> createBuffer(u32 size, u32 usage, u32 flags);


//    private:

        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _device;

        CommandBufferList* _commands;

    };

}

#endif //CALA_CONTEXT_H
