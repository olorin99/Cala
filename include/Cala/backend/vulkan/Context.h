#ifndef CALA_CONTEXT_H
#define CALA_CONTEXT_H

#include <vulkan/vulkan.h>

#include <Ende/Span.h>

namespace cala::backend::vulkan {

    class Context {
    public:

        Context(ende::Span<const char*> extensions);

        ~Context();

        u32 queueIndex(u32 flags);

        VkQueue getQueue(u32 flags);

//    private:

        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _device;

    };

}

#endif //CALA_CONTEXT_H
