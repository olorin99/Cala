#ifndef CALA_OFFLINEPLATFORM_H
#define CALA_OFFLINEPLATFORM_H

#include "volk/volk.h"
#include "Platform.h"

namespace cala::vk {

    class OfflinePlatform : public cala::vk::Platform {
    public:

        OfflinePlatform(u32 width, u32 height, u32 flags = 0);

        std::vector<const char*> requiredExtensions() override;

        VkSurfaceKHR surface(VkInstance instance) override;

        std::pair<u32, u32> windowSize() const override;

    private:

        u32 _width, _height;

    };

}

#endif //CALA_OFFLINEPLATFORM_H
