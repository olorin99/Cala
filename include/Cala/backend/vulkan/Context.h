#ifndef CALA_CONTEXT_H
#define CALA_CONTEXT_H

#include <vulkan/vulkan.h>

#include <Ende/Vector.h>
#include <Ende/Span.h>
#include <Cala/backend/primitives.h>
#include <Cala/backend/vulkan/Platform.h>

#include <Cala/backend/vulkan/vk_mem_alloc.h>

namespace cala::backend::vulkan {

    class Context {
    public:

        Context(cala::backend::Platform& platform);

        ~Context();

        VkDeviceMemory allocate(u32 size, u32 typeBits, MemoryProperties flags);


        void beginDebugLabel(VkCommandBuffer buffer, std::string_view label, std::array<f32, 4> colour) const;

        void endDebugLabel(VkCommandBuffer buffer) const;


        //internal objects
        u32 queueIndex(QueueType type) const;

        VkQueue getQueue(QueueType type) const;

        VkInstance instance() const { return _instance; }

        VkPhysicalDevice physicalDevice() const { return _physicalDevice; }

        VkDevice device() const { return _device; }

        VmaAllocator allocator() const { return _allocator; }

        Format depthFormat() const { return _depthFormat; }

        VkQueryPool queryPool() const { return _queryPool; }


        //query info
        u32 apiVersion() const { return _apiVersion; }

        u32 driverVersion() const { return _driverVersion; }

        const char* vendor() const { return _vendor; }

        PhysicalDeviceType deviceType() const { return _deviceType; }

        const char* deviceTypeString() const;

        ende::Span<char> deviceName() const { return _deviceName; }



        //maybe deprecate
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

    private:

        u32 memoryIndex(u32 filter, VkMemoryPropertyFlags properties);

        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _device;
        VkDebugUtilsMessengerEXT _debugMessenger;
        VmaAllocator _allocator;

        Format _depthFormat;

        VkQueue _graphicsQueue;
        VkQueue _computeQueue;

        VkQueryPool _queryPool;

        // context info
        u32 _apiVersion;
        u32 _driverVersion;
        const char* _vendor;
        PhysicalDeviceType _deviceType;
        ende::Span<char> _deviceName;
        float _timestampPeriod;

    };

}

#endif //CALA_CONTEXT_H
