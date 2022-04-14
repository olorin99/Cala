#ifndef CALA_CONTEXT_H
#define CALA_CONTEXT_H

#include <vulkan/vulkan.h>

#include <Ende/Vector.h>
#include <Ende/Span.h>
#include <Cala/backend/primitives.h>
#include "Platform.h"

namespace cala::backend::vulkan {

    class Context {
    public:

        Context(cala::backend::Platform& platform);

        ~Context();

        u32 queueIndex(u32 flags) const;

        VkQueue getQueue(u32 flags) const;

        u32 memoryIndex(u32 filter, VkMemoryPropertyFlags properties);


        VkDeviceMemory allocate(u32 size, u32 typeBits, MemoryProperties flags);

        std::pair<VkBuffer, VkDeviceMemory> createBuffer(u32 size, u32 usage, MemoryProperties flags);

        struct CreateImage {
            VkImageType imageType;
            VkFormat format;
            VkImageUsageFlags usage;
            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            u32 mipLevels = 1;
            u32 arrayLayers = 1;
        };
        std::pair<VkImage, VkDeviceMemory> createImage(const CreateImage info);

        VkInstance instance() const { return _instance; }

        VkPhysicalDevice physicalDevice() const { return _physicalDevice; }

        VkDevice device() const { return _device; }

        Format depthFormat() const { return _depthFormat; }


        u32 apiVersion() const { return _apiVersion; }

        u32 driverVersion() const { return _driverVersion; }

        const char* vendor() const { return _vendor; }

        PhysicalDeviceType deviceType() const { return _deviceType; }

        const char* deviceTypeString() const;

        ende::Span<char> deviceName() const { return _deviceName; }

    private:

        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _device;
        VkDebugUtilsMessengerEXT _debugMessenger;

        Format _depthFormat;

        u32 _apiVersion;
        u32 _driverVersion;
        const char* _vendor;
        PhysicalDeviceType _deviceType;
        ende::Span<char> _deviceName;

    };

}

#endif //CALA_CONTEXT_H
