#ifndef CALA_PLATFORM_H
#define CALA_PLATFORM_H

#include <vulkan/vulkan.h>
#include <Ende/platform.h>
#include <Ende/Vector.h>

namespace cala::backend {

    class Platform {
    public:

        virtual ~Platform() = default;

        virtual ende::Vector<const char*> requiredExtensions() { return {}; }

        virtual VkSurfaceKHR surface(VkInstance instance) { return VK_NULL_HANDLE; }

        virtual std::pair<u32, u32> windowSize() const { return {0, 0}; }

    protected:


    };

}

#endif //CALA_PLATFORM_H
