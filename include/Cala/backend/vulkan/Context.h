#ifndef CALA_CONTEXT_H
#define CALA_CONTEXT_H

#include <volk.h>
#include <string>
#include <span>
#include <functional>
#include <expected>
#include <Cala/backend/primitives.h>
#include <Cala/backend/vulkan/Platform.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

#define VK_TRY(x) { \
    VkResult result = x; \
    assert(result == VK_SUCCESS); \
}

namespace cala::backend::vulkan {

    class Device;

    struct Limits {
        u32 maxImageDimensions1D = 0;
        u32 maxImageDimensions2D = 0;
        u32 maxImageDimensions3D = 0;
        u32 maxImageDimensionsCube = 0;

        u32 maxDescriptorSetSamplers = 0;
        u32 maxDescriptorSetUniformBuffers = 0;
        u32 maxDescriptorSetStorageBuffers = 0;
        u32 maxDescriptorSetSampledImages = 0;
        u32 maxDescriptorSetStorageImages = 0;

        u32 maxBindlessSamplers = 0;
        u32 maxBindlessUniformBuffers = 0;
        u32 maxBindlessStorageBuffers = 0;
        u32 maxBindlessSampledImages = 0;
        u32 maxBindlessStorageImages = 0;

        f32 maxSamplerAnisotropy = 0;

        f32 timestampPeriod = 0;
    };

    struct DeviceProperties {
        u32 apiVersion;
        u32 driverVersion;
        u32 vendorID;
        std::string vendorName;
        u32 deviceID;
        PhysicalDeviceType deviceType;
        std::string deviceName;
        Limits deviceLimits;
    };

    struct DeviceFeatures {
        bool meshShader = false;
        bool rayTracing = false;
    };

    struct Extensions {
        bool KHR_swapchain = false;
        bool KHR_shader_draw_parameters = false;
        bool KHR_acceleration_structure = false;
        bool KHR_ray_tracing_pipeline = false;
        bool KHR_ray_query = false;
        bool KHR_pipeline_library = false;
        bool KHR_deferred_host_operations = false;

        bool EXT_debug_report = false;
        bool EXT_debug_marker = false;
        bool EXT_memory_budget = false;
        bool EXT_mesh_shader = false;
        bool EXT_load_store_op_none = false;

        bool AMD_buffer_marker = false;
        bool AMD_device_coherent_memory = false;
    };

    class Context {
    public:

//        Context(Device* device, cala::backend::Platform& platform);

        struct ContextInfo {
            std::string applicationName = {};
            std::string engineName = "Cala";
            u32 apiVersion = VK_API_VERSION_1_3;
            std::span<const char* const> instanceExtensions = {};
            std::span<const char* const> deviceExtensions = {};
            std::function<u32(const DeviceProperties&)> selector = {};
        };
        static std::expected<Context, Error> create(Device* device, ContextInfo info);

        Context() = default;

        ~Context();

        Context(Context&& rhs) noexcept;

        Context& operator=(Context&& rhs) noexcept;


        void beginDebugLabel(VkCommandBuffer buffer, std::string_view label, std::array<f32, 4> colour) const;

        void endDebugLabel(VkCommandBuffer buffer) const;

        void setDebugName(u32 type, u64 object, std::string_view name) const;


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
        u32 apiVersion() const { return _deviceProperties.apiVersion; }

        u32 driverVersion() const { return _deviceProperties.driverVersion; }

        std::string_view vendor() const { return _deviceProperties.vendorName; }

        PhysicalDeviceType deviceType() const { return _deviceProperties.deviceType; }

        const char* deviceTypeString() const;

        std::string_view deviceName() const { return _deviceProperties.deviceName; }

        f32 timestampPeriod() const { return getLimits().timestampPeriod; }

        f32 maxAnisotropy() const { return getLimits().maxSamplerAnisotropy; }

        struct PipelineStatistics {
            u64 inputAssemblyVertices = 0;
            u64 inputAssemblyPrimitives = 0;
            u64 vertexShaderInvocations = 0;
            u64 clippingInvocations = 0;
            u64 clippingPrimitives = 0;
            u64 fragmentShaderInvocations = 0;
        };

        PipelineStatistics getPipelineStatistics() const;

        Extensions getSupportedExtensions() const { return _supportedExtensions; }

        struct Features {
            bool meshShading = false;
            bool deviceAddress = false;
            bool sync2 = false;
        };

        Features getEnabledFeatures() const { return _enabledFeatures; }

        Limits getLimits() const { return _deviceProperties.deviceLimits; }

    private:

        u32 memoryIndex(u32 filter, VkMemoryPropertyFlags properties);

        Device* _device = nullptr;
        VkInstance _instance = VK_NULL_HANDLE;
        VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        VkDevice _logicalDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
        VmaAllocator _allocator = VK_NULL_HANDLE;

        Format _depthFormat = Format::UNDEFINED;

        VkQueue _graphicsQueue = VK_NULL_HANDLE;
        VkQueue _computeQueue = VK_NULL_HANDLE;
        VkQueue _transferQueue = VK_NULL_HANDLE;
        VkQueue _presentQueue = VK_NULL_HANDLE;

        VkQueryPool _timestampQueryPool = VK_NULL_HANDLE;
        VkQueryPool _pipelineStatistics = VK_NULL_HANDLE;
        u64 _pipelineStats[6] = {};

        DeviceProperties _deviceProperties = {};

        Extensions _supportedExtensions = {};
        Features _enabledFeatures = {};

    };

}

#endif //CALA_CONTEXT_H
