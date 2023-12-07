#include <cstdio> //vma complains without this
#include <Cala/vulkan/Context.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

#include <Cala/vulkan/primitives.h>
#include <Cala/vulkan/Device.h>

class VulkanContextException : public std::exception {
public:

    explicit VulkanContextException(const char* msg)
        : _msg("Unable to create Vulkan Device: ")
    {
        _msg += msg;
    }

    virtual ~VulkanContextException() noexcept {}

    virtual const char* what() const noexcept {
        return _msg.c_str();
    }

protected:

    std::string _msg;

};

const char* validationLayers[] = {
        "VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
        ) {
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        return VK_FALSE;
    auto d = reinterpret_cast<cala::vk::Device*>(pUserData);
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            d->logger().info("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            d->logger().info("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            d->logger().warn("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            d->logger().error("{}", pCallbackData->pMessage);
            break;
        default:
            break;
    }
    return VK_FALSE;
}

bool checkDeviceSuitable(VkPhysicalDevice device, VkPhysicalDeviceFeatures* deviceFeatures) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

//    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, deviceFeatures);
    return deviceFeatures->geometryShader;
}

cala::vk::DeviceProperties getDeviceProperties(VkPhysicalDevice physicalDevice) {
    cala::vk::DeviceProperties properties{};

    VkPhysicalDeviceProperties2 deviceProperties2{};
    VkPhysicalDeviceDescriptorIndexingProperties indexingProperties{};
    indexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &indexingProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    VkPhysicalDeviceProperties deviceProperties = deviceProperties2.properties;

    properties.apiVersion = deviceProperties.apiVersion;
    properties.driverVersion = deviceProperties.driverVersion;
    properties.vendorID = deviceProperties.vendorID;
    switch (deviceProperties.vendorID) {
        case 0x1002:
            properties.vendorName = "AMD";
            break;
        case 0x1010:
            properties.vendorName = "ImgTec";
            break;
        case 0x10DE:
            properties.vendorName = "NVIDIA";
            break;
        case 0x13B5:
            properties.vendorName = "ARM";
            break;
        case 0x5143:
            properties.vendorName = "Qualcomm";
            break;
        case 0x8086:
            properties.vendorName = "INTEL";
            break;
    }
    properties.deviceID = deviceProperties.deviceID;
    properties.deviceType = static_cast<cala::vk::PhysicalDeviceType>(deviceProperties.deviceType);
    properties.deviceName = deviceProperties.deviceName;

    properties.deviceLimits.maxImageDimensions1D = deviceProperties.limits.maxImageDimension1D;
    properties.deviceLimits.maxImageDimensions2D = deviceProperties.limits.maxImageDimension2D;
    properties.deviceLimits.maxImageDimensions3D = deviceProperties.limits.maxImageDimension3D;
    properties.deviceLimits.maxImageDimensionsCube = deviceProperties.limits.maxImageDimensionCube;

    properties.deviceLimits.maxDescriptorSetSamplers = deviceProperties.limits.maxDescriptorSetSamplers;
    properties.deviceLimits.maxDescriptorSetUniformBuffers = deviceProperties.limits.maxDescriptorSetUniformBuffers;
    properties.deviceLimits.maxDescriptorSetStorageBuffers = deviceProperties.limits.maxDescriptorSetStorageBuffers;
    properties.deviceLimits.maxDescriptorSetSampledImages = deviceProperties.limits.maxDescriptorSetSampledImages;
    properties.deviceLimits.maxDescriptorSetStorageImages = deviceProperties.limits.maxDescriptorSetStorageImages;

    u32 maxBindlessCount = 1 << 16;

    // Check against limits for case when driver doesnt report correct correct values (e.g. amdgpu-pro on linux)
    properties.deviceLimits.maxBindlessSamplers = indexingProperties.maxDescriptorSetUpdateAfterBindSamplers < std::numeric_limits<u32>::max() - 10000 ? std::min(indexingProperties.maxDescriptorSetUpdateAfterBindSamplers - 100, maxBindlessCount) : 10000;
    properties.deviceLimits.maxBindlessUniformBuffers = indexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers < std::numeric_limits<u32>::max() - 10000 ? std::min(indexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers - 100, maxBindlessCount) : 1000;
    properties.deviceLimits.maxBindlessStorageBuffers = indexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers < std::numeric_limits<u32>::max() - 10000 ? std::min(indexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers - 100, maxBindlessCount) : 1000;
    properties.deviceLimits.maxBindlessSampledImages = indexingProperties.maxDescriptorSetUpdateAfterBindSampledImages < std::numeric_limits<u32>::max() - 10000 ? std::min(indexingProperties.maxDescriptorSetUpdateAfterBindSampledImages - 100, maxBindlessCount) : 1000;
    properties.deviceLimits.maxBindlessStorageImages = indexingProperties.maxDescriptorSetUpdateAfterBindStorageImages < std::numeric_limits<u32>::max() - 10000 ? std::min(indexingProperties.maxDescriptorSetUpdateAfterBindStorageImages - 100, maxBindlessCount) : 1000;

    properties.deviceLimits.maxSamplerAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;

    properties.deviceLimits.timestampPeriod = deviceProperties.limits.timestampPeriod;
    return properties;
}

template <typename T, typename U>
void appendFeatureChain(T* start, U* next) {
    auto* oldNext = start->pNext;
    start->pNext = next;
    next->pNext = oldNext;
}

std::expected<cala::vk::Context, cala::vk::Error> cala::vk::Context::create(cala::vk::Device* device, cala::vk::Context::ContextInfo info) {
    Context context;
    context._device = device;

    VK_TRY(volkInitialize());

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = info.applicationName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = info.engineName.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    assert(info.apiVersion >= VK_API_VERSION_1_3);
    appInfo.apiVersion = info.apiVersion;

    std::vector<const char*> instanceExtensions;
    instanceExtensions.insert(instanceExtensions.begin(), info.instanceExtensions.begin(), info.instanceExtensions.end());
#ifndef NDEBUG
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    VkValidationFeatureEnableEXT v[5] = {VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};
    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.pEnabledValidationFeatures = v;
    validationFeatures.enabledValidationFeatureCount = 1;

    //create instance with required instanceExtensions
    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    instanceCreateInfo.enabledExtensionCount = instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VK_TRY(vkCreateInstance(&instanceCreateInfo, nullptr, &context._instance));

    volkLoadInstanceOnly(context._instance);

    //create debug messenger
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    debugInfo.pUserData = device;

    auto createDebugUtils = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context._instance, "vkCreateDebugUtilsMessengerEXT");
    VK_TRY(createDebugUtils(context._instance, &debugInfo, nullptr, &context._debugMessenger));
#endif

    //get physical device
    u32 deviceCount = 0;
    VK_TRY(vkEnumeratePhysicalDevices(context._instance, &deviceCount, nullptr));
    if (deviceCount == 0)
        return std::unexpected(Error::INVALID_GPU);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    std::vector<u32> deviceScores(deviceCount);
    VK_TRY(vkEnumeratePhysicalDevices(context._instance, &deviceCount, devices.data()));

    const auto defaultSelector = [](const DeviceProperties& properties) -> u32 {
        switch (properties.deviceType) {
            case cala::vk::PhysicalDeviceType::DISCRETE_GPU:
                return 1000;
            case cala::vk::PhysicalDeviceType::INTEGRATED_GPU:
                return 100;
        }
        return 0;
    };

    u32 maxIndex = 0;
    u32 maxScore = 0;
    for (u32 i = 0; i < deviceCount; i++) {
        auto properties = getDeviceProperties(devices[i]);
        u32 score = info.selector ? info.selector(properties) : defaultSelector(properties);
        deviceScores[i] = score;
        if (score > maxScore) {
            maxScore = score;
            maxIndex = i;
            context._deviceProperties = properties;
        }
    }
    context._physicalDevice = devices[maxIndex];

    if (context._physicalDevice == VK_NULL_HANDLE)
        return std::unexpected(Error::INVALID_GPU);


    // enable extensions
    u32 supportedDeviceExtensionsCount = 0;
    vkEnumerateDeviceExtensionProperties(context._physicalDevice, nullptr, &supportedDeviceExtensionsCount, nullptr);
    std::vector<VkExtensionProperties> supportedDeviceExtensions(supportedDeviceExtensionsCount);
    vkEnumerateDeviceExtensionProperties(context._physicalDevice, nullptr, &supportedDeviceExtensionsCount, supportedDeviceExtensions.data());

    auto checkExtension = [](const std::vector<VkExtensionProperties>& supportedExtensions, const char* name) {
        for (auto& extension : supportedExtensions) {
            if (strcmp(extension.extensionName, name) == 0)
                return true;
        }
        return false;
    };

    std::vector<const char*> deviceExtensions;
    for (auto& extension : info.deviceExtensions) {
        if (checkExtension(supportedDeviceExtensions, extension)) {
            deviceExtensions.push_back(extension);
        }
    }

    {
        context._supportedExtensions.KHR_swapchain = checkExtension(supportedDeviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        context._supportedExtensions.KHR_shader_draw_parameters = checkExtension(supportedDeviceExtensions, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
        context._supportedExtensions.KHR_acceleration_structure = checkExtension(supportedDeviceExtensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        context._supportedExtensions.KHR_ray_tracing_pipeline = checkExtension(supportedDeviceExtensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        context._supportedExtensions.KHR_ray_query = checkExtension(supportedDeviceExtensions, VK_KHR_RAY_QUERY_EXTENSION_NAME);
        context._supportedExtensions.KHR_pipeline_library = checkExtension(supportedDeviceExtensions, VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        context._supportedExtensions.KHR_deferred_host_operations = checkExtension(supportedDeviceExtensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

        context._supportedExtensions.EXT_debug_report = checkExtension(supportedDeviceExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        context._supportedExtensions.EXT_debug_marker = context._supportedExtensions.EXT_debug_report && checkExtension(supportedDeviceExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        context._supportedExtensions.EXT_memory_budget = checkExtension(supportedDeviceExtensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        context._supportedExtensions.EXT_mesh_shader = checkExtension(supportedDeviceExtensions, VK_EXT_MESH_SHADER_EXTENSION_NAME);
        context._supportedExtensions.EXT_load_store_op_none = checkExtension(supportedDeviceExtensions, VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME);

        context._supportedExtensions.AMD_buffer_marker = checkExtension(supportedDeviceExtensions, VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
        context._supportedExtensions.AMD_device_coherent_memory = checkExtension(supportedDeviceExtensions, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);
    }

//    _supportedExtensions.AMD_buffer_marker = false;

    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (context._supportedExtensions.EXT_memory_budget)
        deviceExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
#ifndef NDEBUG
    if (context._supportedExtensions.EXT_debug_marker)
        deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    if (context._supportedExtensions.AMD_buffer_marker)
        deviceExtensions.push_back(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
    if (context._supportedExtensions.AMD_device_coherent_memory)
        deviceExtensions.push_back(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);
#endif


    u32 queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context._physicalDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> familyProperties(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context._physicalDevice, &queueCount, familyProperties.data());

    u32 priCount = std::max_element(familyProperties.begin(), familyProperties.end(), [](const auto& a, const auto& b) {
        return a.queueCount < b.queueCount;
    })->queueCount;

    std::vector<f32> priorities(priCount, 1.f);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (u32 i = 0; i < familyProperties.size(); i++) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = i;
        queueCreateInfo.queueCount = familyProperties[i].queueCount;
        queueCreateInfo.pQueuePriorities = priorities.data();

        queueCreateInfos.push_back(queueCreateInfo);
    }


    // enable features
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
//    deviceFeatures2.features.geometryShader = VK_TRUE;
//    deviceFeatures2.features.multiDrawIndirect = VK_TRUE;
//    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
//    deviceFeatures2.features.depthClamp = VK_TRUE;
//    deviceFeatures2.features.depthBiasClamp = VK_TRUE;
//    deviceFeatures2.features.depthBounds = VK_TRUE;
//    deviceFeatures2.features.pipelineStatisticsQuery = VK_TRUE;
//    deviceFeatures2.features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
//    deviceFeatures2.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
//    deviceFeatures2.features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
//    deviceFeatures2.features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
//    deviceFeatures2.features.shaderInt64 = VK_TRUE;
//    deviceFeatures2.features.shaderResourceMinLod = VK_TRUE;

    vkGetPhysicalDeviceFeatures2(context._physicalDevice, &deviceFeatures2);

    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    appendFeatureChain(&deviceFeatures2, &vulkan11Features);

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    vulkan12Features.drawIndirectCount = VK_TRUE;
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    vulkan12Features.hostQueryReset = VK_TRUE;
    vulkan12Features.timelineSemaphore = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.scalarBlockLayout = VK_TRUE;
    appendFeatureChain(&deviceFeatures2, &vulkan12Features);

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.synchronization2 = VK_TRUE;
    appendFeatureChain(&deviceFeatures2, &vulkan13Features);

    //TODO: mesh shader and raytracing features

    // create device
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();

    createInfo.pEnabledFeatures = nullptr;
    createInfo.pNext = &deviceFeatures2;

    createInfo.enabledExtensionCount = deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
#ifndef NDEBUG
//    createInfo.enabledLayerCount = 1;
//    createInfo.ppEnabledLayerNames = validationLayers;
#endif
    VK_TRY(vkCreateDevice(context._physicalDevice, &createInfo, nullptr, &context._logicalDevice));

    volkLoadDevice(context._logicalDevice);

    context._enabledFeatures.meshShading = false;
    context._enabledFeatures.deviceAddress = vulkan12Features.bufferDeviceAddress;
    context._enabledFeatures.sync2 = vulkan13Features.synchronization2;

    // init VMA
    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.vulkanApiVersion = appInfo.apiVersion;
    allocatorCreateInfo.physicalDevice = context._physicalDevice;
    allocatorCreateInfo.device = context._logicalDevice;
    allocatorCreateInfo.instance = context._instance;

    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr               = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr                 = vkGetDeviceProcAddr;
    vulkanFunctions.vkAllocateMemory                    = vkAllocateMemory;
    vulkanFunctions.vkBindBufferMemory                  = vkBindBufferMemory;
    vulkanFunctions.vkBindBufferMemory2KHR              = vkBindBufferMemory2;
    vulkanFunctions.vkBindImageMemory                   = vkBindImageMemory;
    vulkanFunctions.vkBindImageMemory2KHR               = vkBindImageMemory2;
    vulkanFunctions.vkCreateBuffer                      = vkCreateBuffer;
    vulkanFunctions.vkCreateImage                       = vkCreateImage;
    vulkanFunctions.vkDestroyBuffer                     = vkDestroyBuffer;
    vulkanFunctions.vkDestroyImage                      = vkDestroyImage;
    vulkanFunctions.vkFlushMappedMemoryRanges           = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkFreeMemory                        = vkFreeMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements       = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR   = vkGetBufferMemoryRequirements2;
    vulkanFunctions.vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR    = vkGetImageMemoryRequirements2;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
    vulkanFunctions.vkGetPhysicalDeviceProperties       = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkInvalidateMappedMemoryRanges      = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkMapMemory                         = vkMapMemory;
    vulkanFunctions.vkUnmapMemory                       = vkUnmapMemory;
    vulkanFunctions.vkCmdCopyBuffer                     = vkCmdCopyBuffer;
    vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
    vulkanFunctions.vkGetDeviceImageMemoryRequirements  = vkGetDeviceImageMemoryRequirements;

    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    if (context._supportedExtensions.EXT_memory_budget)
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
#ifndef NDEBUG
    if (context._supportedExtensions.AMD_device_coherent_memory)
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
#endif
    if (vulkan12Features.bufferDeviceAddress)
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&allocatorCreateInfo, &context._allocator);

    //get depth format supported for swapchain
    for (VkFormat format : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        VkFormatProperties depthProperties;
        vkGetPhysicalDeviceFormatProperties(context._physicalDevice, format, &depthProperties);

        if ((depthProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            context._depthFormat = static_cast<Format>(format);
            break;
        }
    }

    //cache queues for later use
    u32 queueIndices[4] = {};
    if (context.queueIndex(queueIndices[0], QueueType::GRAPHICS))
        vkGetDeviceQueue(context._logicalDevice, queueIndices[0], 0, &context._graphicsQueue);

    if (context.queueIndex(queueIndices[1], QueueType::COMPUTE, QueueType::GRAPHICS | QueueType::TRANSFER) ||
        context.queueIndex(queueIndices[1], QueueType::COMPUTE, QueueType::GRAPHICS))
        vkGetDeviceQueue(context._logicalDevice, queueIndices[1], 0, &context._computeQueue);

    if (context.queueIndex(queueIndices[2], QueueType::TRANSFER, QueueType::GRAPHICS | QueueType::COMPUTE) ||
        context.queueIndex(queueIndices[2], QueueType::TRANSFER, QueueType::GRAPHICS))
        vkGetDeviceQueue(context._logicalDevice, queueIndices[2], 0, &context._transferQueue);

    if (context.queueIndex(queueIndices[3], QueueType::PRESENT))
        vkGetDeviceQueue(context._logicalDevice, queueIndices[3], 0, &context._presentQueue);

    context.setDebugName(VK_OBJECT_TYPE_QUEUE, (u64)context._graphicsQueue, "GraphicsQueue");
    if (context._graphicsQueue != context._computeQueue && context._computeQueue != VK_NULL_HANDLE)
        context.setDebugName(VK_OBJECT_TYPE_QUEUE, (u64)context._computeQueue, "ComputeQueue");
    if (context._graphicsQueue != context._computeQueue && context._computeQueue != context._transferQueue && context._transferQueue != VK_NULL_HANDLE)
        context.setDebugName(VK_OBJECT_TYPE_QUEUE, (u64)context._transferQueue, "TransferQueue");

    VkQueryPoolCreateInfo queryPoolCreateInfo{};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.queryCount = 50 * 2;
    context._timestampQueryPool = VK_NULL_HANDLE;
    VK_TRY(vkCreateQueryPool(context._logicalDevice, &queryPoolCreateInfo, nullptr, &context._timestampQueryPool));

    const char* pipelineStatNames[] = {
            "Input Assembly vertex count",
            "Input Assembly primitives count",
            "Vertex Shader invocations",
            "Clipping stage primitives processed",
            "Clipping stage primitives output",
            "Fragment Shader invocations"
    };

    VkQueryPoolCreateInfo pipelineStatisticsCreate{};
    pipelineStatisticsCreate.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pipelineStatisticsCreate.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    pipelineStatisticsCreate.pipelineStatistics =
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
    pipelineStatisticsCreate.queryCount = 6;
    VK_TRY(vkCreateQueryPool(context._logicalDevice, &pipelineStatisticsCreate, nullptr, &context._pipelineStatistics));
    vkResetQueryPool(context._logicalDevice, context._pipelineStatistics, 0, pipelineStatisticsCreate.queryCount);
    vkResetQueryPool(context._logicalDevice, context._timestampQueryPool, 0, queryPoolCreateInfo.queryCount);

    return context;
}

cala::vk::Context::~Context() {
    if (!_logicalDevice)
        return;
    vkDestroyQueryPool(_logicalDevice, _pipelineStatistics, nullptr);
    vkDestroyQueryPool(_logicalDevice, _timestampQueryPool, nullptr);
    vmaDestroyAllocator(_allocator);
    vkDestroyDevice(_logicalDevice, nullptr);

#ifndef NDEBUG
    auto destroyDebugUtils = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
    destroyDebugUtils(_instance, _debugMessenger, nullptr);
#endif

    vkDestroyInstance(_instance, nullptr);
}

cala::vk::Context::Context(cala::vk::Context &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_instance, rhs._instance);
    std::swap(_physicalDevice, rhs._physicalDevice);
    std::swap(_logicalDevice, rhs._logicalDevice);
    std::swap(_debugMessenger, rhs._debugMessenger);
    std::swap(_allocator, rhs._allocator);
    std::swap(_depthFormat, rhs._depthFormat);
    std::swap(_graphicsQueue, rhs._graphicsQueue);
    std::swap(_computeQueue, rhs._computeQueue);
    std::swap(_transferQueue, rhs._transferQueue);
    std::swap(_presentQueue, rhs._presentQueue);
    std::swap(_timestampQueryPool, rhs._timestampQueryPool);
    std::swap(_pipelineStatistics, rhs._pipelineStatistics);
    std::swap(_deviceProperties, rhs._deviceProperties);
    std::swap(_supportedExtensions, rhs._supportedExtensions);
    std::swap(_enabledFeatures, rhs._enabledFeatures);
}

cala::vk::Context &cala::vk::Context::operator=(cala::vk::Context &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_instance, rhs._instance);
    std::swap(_physicalDevice, rhs._physicalDevice);
    std::swap(_logicalDevice, rhs._logicalDevice);
    std::swap(_debugMessenger, rhs._debugMessenger);
    std::swap(_allocator, rhs._allocator);
    std::swap(_depthFormat, rhs._depthFormat);
    std::swap(_graphicsQueue, rhs._graphicsQueue);
    std::swap(_computeQueue, rhs._computeQueue);
    std::swap(_transferQueue, rhs._transferQueue);
    std::swap(_presentQueue, rhs._presentQueue);
    std::swap(_timestampQueryPool, rhs._timestampQueryPool);
    std::swap(_pipelineStatistics, rhs._pipelineStatistics);
    std::swap(_deviceProperties, rhs._deviceProperties);
    std::swap(_supportedExtensions, rhs._supportedExtensions);
    std::swap(_enabledFeatures, rhs._enabledFeatures);
    return *this;
}



void cala::vk::Context::beginDebugLabel(VkCommandBuffer buffer, std::string_view label, std::array<f32, 4> colour) const {
#ifndef NDEBUG
//    static auto beginDebugLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(_instance, "vkCmdBeginDebugUtilsLabelEXT");
    VkDebugUtilsLabelEXT labelExt{};
    labelExt.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelExt.pLabelName = label.data();
    for (i32 i = 0; i < 4; i++)
        labelExt.color[i] = colour[i];
    vkCmdBeginDebugUtilsLabelEXT(buffer, &labelExt);
//    beginDebugLabel(buffer, &labelExt);
#endif
}

void cala::vk::Context::endDebugLabel(VkCommandBuffer buffer) const {
#ifndef NDEBUG
//    static auto endDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(_instance, "vkCmdEndDebugUtilsLabelEXT");
//    endDebugLabel(buffer);
    vkCmdEndDebugUtilsLabelEXT(buffer);
#endif
}


void cala::vk::Context::setDebugName(u32 type, u64 object, std::string_view name) const {
#ifndef NDEBUG
    VkDebugUtilsObjectNameInfoEXT objectNameInfo{};
    objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.objectType = (VkObjectType)type;
    objectNameInfo.pObjectName = name.begin();
    objectNameInfo.objectHandle = object;
    vkSetDebugUtilsObjectNameEXT(_logicalDevice, &objectNameInfo);
#endif
}


bool cala::vk::Context::queueIndex(u32& index, QueueType type, QueueType rejectType) const {
    u32 flags = static_cast<u32>(type);
    u32 rejected = static_cast<u32>(rejectType);
    if (flags & 0x20) {
        index = 0;
        return true;
    }

    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> familyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, familyProperties.data());

    u32 i = 0;
    for (auto& queueFamily : familyProperties) {

        if ((queueFamily.queueFlags & rejected) != 0) {
            i++;
            continue;
        }

        if (queueFamily.queueCount > 0) {
            if ((queueFamily.queueFlags & flags) == flags) {
                index = i;
                return true;
            }
        }
        i++;
    }
    return false;
}

VkQueue cala::vk::Context::getQueue(QueueType type) const {
    switch (type) {
        case QueueType::GRAPHICS:
            return _graphicsQueue;
        case QueueType::COMPUTE:
            return _computeQueue;
        case QueueType::TRANSFER:
            return _transferQueue;
        case QueueType::PRESENT:
            return _presentQueue;
        default:
            break;
    }
    VkQueue queue;
    u32 index = 0;
    queueIndex(index, type);
    vkGetDeviceQueue(_logicalDevice, index, 0, &queue);
    return queue;
}

u32 cala::vk::Context::memoryIndex(u32 filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((filter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    return 0; //TODO: error on none available
}


const char *cala::vk::Context::deviceTypeString() const {
    switch (_deviceProperties.deviceType) {
        case PhysicalDeviceType::OTHER:
            return "OTHER";
        case PhysicalDeviceType::INTEGRATED_GPU:
            return "INTEGRATED_GPU";
        case PhysicalDeviceType::DISCRETE_GPU:
            return "DISCRETE_GPU";
        case PhysicalDeviceType::VIRTUAL_GPU:
            return "VIRTUAL_GPU";
        case PhysicalDeviceType::CPU:
            return "CPU";
    }
    return "OTHER";
}

cala::vk::Context::PipelineStatistics cala::vk::Context::getPipelineStatistics() const {
    vkGetQueryPoolResults(_logicalDevice, _pipelineStatistics, 0, 1, 6 * sizeof(u64), (void*)_pipelineStats, sizeof(u64), VK_QUERY_RESULT_64_BIT);
    return {
        _pipelineStats[0],
        _pipelineStats[1],
        _pipelineStats[2],
        _pipelineStats[3],
        _pipelineStats[4],
        _pipelineStats[5]
    };
}