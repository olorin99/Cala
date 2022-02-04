#ifndef CALA_PLATFORM_H
#define CALA_PLATFORM_H

#include <vulkan/vulkan.h>
#include "Ende/Vector.h"

namespace cala::backend {

    class Platform {
    public:

        virtual ~Platform() = default;

        virtual ende::Vector<const char*> requiredExtensions() { return {}; }

        virtual VkSurfaceKHR surface(VkInstance instance) { return VK_NULL_HANDLE; }

    protected:


    };

}

#endif //CALA_PLATFORM_H
