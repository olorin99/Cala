#ifndef CALA_CONTEXT_H
#define CALA_CONTEXT_H

#include <vulkan/vulkan.h>
#include <string>
#include <Ende/Vector.h>
#include <Ende/Span.h>
#include <Cala/backend/primitives.h>
#include <Cala/backend/vulkan/Platform.h>

#include "../../third_party/vk_mem_alloc.h"

#define VK_TRY(x) { \
    VkResult result = x; \
    assert(result == VK_SUCCESS); \
}

namespace cala::backend::vulkan {

    class Device;

    class Context {
    public:

        Context(Device* device, cala::backend::Platform& platform);

        ~Context();


        void beginDebugLabel(VkCommandBuffer buffer, std::string_view label, std::array<f32, 4> colour) const;

        void endDebugLabel(VkCommandBuffer buffer) const;


        //internal objects
        bool queueIndex(u32& index, QueueType type, QueueType rejectType = QueueType::NONE) const;

        VkQueue getQueue(QueueType type) const;

        VkInstance instance() const { return _instance; }

        VkPhysicalDevice physicalDevice() const { return _physicalDevice; }

        VkDevice device() const { return _logicalDevice; }

        VmaAllocator allocator() const { return _allocator; }

        Format depthFormat() const { return _depthFormat; }

        VkQueryPool timestampPool() const { return _timestampQueryPool; }

        VkQueryPool pipelineStatisticsPool() const { return _pipelineStatistics; }


        //query info
        u32 apiVersion() const { return _apiVersion; }

        u32 driverVersion() const { return _driverVersion; }

        const char* vendor() const { return _vendor; }

        PhysicalDeviceType deviceType() const { return _deviceType; }

        const char* deviceTypeString() const;

        ende::Span<const char> deviceName() const { return _deviceName; }

        f32 timestampPeriod() const { return _timestampPeriod; }

        f32 maxAnisotropy() const { return _maxAnisotropy; }

        struct PipelineStatistics {
            u64 inputAssemblyVertices = 0;
            u64 inputAssemblyPrimitives = 0;
            u64 vertexShaderInvocations = 0;
            u64 clippingInvocations = 0;
            u64 clippingPrimitives = 0;
            u64 fragmentShaderInvocations = 0;
        };

        PipelineStatistics getPipelineStatistics() const;

    private:

        u32 memoryIndex(u32 filter, VkMemoryPropertyFlags properties);

        Device* _device;
        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _logicalDevice;
        VkDebugUtilsMessengerEXT _debugMessenger;
        VmaAllocator _allocator;

        Format _depthFormat;

        VkQueue _graphicsQueue;
        VkQueue _computeQueue;
        VkQueue _transferQueue;
        VkQueue _presentQueue;

        VkQueryPool _timestampQueryPool;
        VkQueryPool _pipelineStatistics;
        u64 _pipelineStats[6];

        // context info
        u32 _apiVersion;
        u32 _driverVersion;
        const char* _vendor;
        PhysicalDeviceType _deviceType;
        std::string _deviceName;
        f32 _timestampPeriod;
        f32 _maxAnisotropy;

    };

}

#endif //CALA_CONTEXT_H
