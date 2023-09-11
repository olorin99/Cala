#include <cstdio> //vma complains without this
#include <Cala/backend/vulkan/Context.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "../third_party/vk_mem_alloc.h"

#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Device.h>
#include <Ende/Vector.h>

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
    auto d = reinterpret_cast<cala::backend::vulkan::Device*>(pUserData);
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

cala::backend::vulkan::Context::Context(cala::backend::vulkan::Device* device, cala::backend::Platform& platform)
    : _device(device)
{
    VK_TRY(volkInitialize());

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cala";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Cala";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    auto instanceExtensions = platform.requiredExtensions();
#ifndef NDEBUG
    instanceExtensions.push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    instanceExtensions.push(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

//    instanceExtensions.push(VK_EXT_validation_features);

//    VkValidationFeatureEnableEXT v[2] = {VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT};
//////    auto v = VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT;
//
//    VkValidationFeaturesEXT validationFeatures{};
//    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
//    validationFeatures.pEnabledValidationFeatures = v;
//    validationFeatures.enabledValidationFeatureCount = 2;

    //create instance with required instanceExtensions
    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    instanceCreateInfo.enabledExtensionCount = instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
//    instanceCreateInfo.pNext = &validationFeatures;

    VK_TRY(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance));

    volkLoadInstanceOnly(_instance);

    //create debug messenger
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    debugInfo.pUserData = _device;

    auto createDebugUtils = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
    VK_TRY(createDebugUtils(_instance, &debugInfo, nullptr, &_debugMessenger));
#endif

    //get physical device
    u32 deviceCount = 0;
    VK_TRY(vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr));
    if (deviceCount == 0)
        throw VulkanContextException("No GPUs found with vulkan support");

    ende::Vector<VkPhysicalDevice> devices(deviceCount);
    VK_TRY(vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data()));

    VkPhysicalDeviceFeatures deviceFeatures;
    for (auto& device : devices) {
        if (checkDeviceSuitable(device, &deviceFeatures)) {
            _physicalDevice = device;
            break;
        }
    }

    if (_physicalDevice == VK_NULL_HANDLE)
        throw VulkanContextException("No GPUs found with required functionality");

    u32 queuCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queuCount, nullptr);
    ende::Vector<VkQueueFamilyProperties> familyProperties(queuCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queuCount, familyProperties.data());

    u32 priCount = std::max_element(familyProperties.begin(), familyProperties.end(), [](const auto& a, const auto& b) {
        return a.queueCount < b.queueCount;
    })->queueCount;

    ende::Vector<f32> priorities(priCount, 1.f);

    ende::Vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (u32 i = 0; i < familyProperties.size(); i++) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = i;
        queueCreateInfo.queueCount = familyProperties[i].queueCount;
        queueCreateInfo.pQueuePriorities = priorities.data();

        queueCreateInfos.push(queueCreateInfo);
    }

    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(_physicalDevice, &deviceProperties);
    _apiVersion = deviceProperties.apiVersion;
    _driverVersion = deviceProperties.driverVersion;
    switch (deviceProperties.vendorID) {
        case 0x1002:
            _vendor = "AMD";
            break;
        case 0x1010:
            _vendor = "ImgTec";
            break;
        case 0x10DE:
            _vendor = "NVIDIA";
            break;
        case 0x13B5:
            _vendor = "ARM";
            break;
        case 0x5143:
            _vendor = "Qualcomm";
            break;
        case 0x8086:
            _vendor = "INTEL";
            break;
    }
    _deviceType = static_cast<PhysicalDeviceType>(deviceProperties.deviceType);
    _deviceName = deviceProperties.deviceName;
    _timestampPeriod = deviceProperties.limits.timestampPeriod;
    _maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;


    u32 supportedDeviceExtensionsCount = 0;
    vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &supportedDeviceExtensionsCount, nullptr);
    ende::Vector<VkExtensionProperties> supportedDeviceExtensions(supportedDeviceExtensionsCount);
    vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &supportedDeviceExtensionsCount, supportedDeviceExtensions.data());

    auto checkExtensions = [](const ende::Vector<VkExtensionProperties>& supportedExtensions, const char* name) {
        for (auto& extension : supportedExtensions) {
            if (strcmp(extension.extensionName, name) == 0)
                return true;
        }
        return false;
    };

    {
        _supportedExtensions.KHR_swapchain = checkExtensions(supportedDeviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        _supportedExtensions.KHR_shader_draw_parameters = checkExtensions(supportedDeviceExtensions, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
        _supportedExtensions.KHR_acceleration_structure = checkExtensions(supportedDeviceExtensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        _supportedExtensions.KHR_ray_tracing_pipeline = checkExtensions(supportedDeviceExtensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        _supportedExtensions.KHR_ray_query = checkExtensions(supportedDeviceExtensions, VK_KHR_RAY_QUERY_EXTENSION_NAME);
        _supportedExtensions.KHR_pipeline_library = checkExtensions(supportedDeviceExtensions, VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        _supportedExtensions.KHR_deferred_host_operations = checkExtensions(supportedDeviceExtensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

        _supportedExtensions.EXT_debug_marker = checkExtensions(supportedDeviceExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        _supportedExtensions.EXT_memory_budget = checkExtensions(supportedDeviceExtensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        _supportedExtensions.EXT_mesh_shader = checkExtensions(supportedDeviceExtensions, VK_EXT_MESH_SHADER_EXTENSION_NAME);

        _supportedExtensions.AMD_buffer_marker = checkExtensions(supportedDeviceExtensions, VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
        _supportedExtensions.AMD_device_coherent_memory = checkExtensions(supportedDeviceExtensions, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);
    }



    ende::Vector<const char*> deviceExtensions;
    deviceExtensions.push(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.push(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
    if (_supportedExtensions.EXT_memory_budget)
        deviceExtensions.push(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
#ifndef NDEBUG
    if (_supportedExtensions.EXT_debug_marker)
        deviceExtensions.push(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    if (_supportedExtensions.AMD_buffer_marker)
        deviceExtensions.push(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
    if (_supportedExtensions.AMD_device_coherent_memory)
        deviceExtensions.push(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
#ifndef NDEBUG
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
#endif

    VkPhysicalDeviceVulkan12Features vulkan12Features{};

    createInfo.pNext = &vulkan12Features;

    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.drawIndirectCount = VK_TRUE;

    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;

    vulkan12Features.hostQueryReset = VK_TRUE;

    VK_TRY(vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_logicalDevice));

    volkLoadDevice(_logicalDevice);

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.vulkanApiVersion = appInfo.apiVersion;
    allocatorCreateInfo.physicalDevice = _physicalDevice;
    allocatorCreateInfo.device = _logicalDevice;
    allocatorCreateInfo.instance = _instance;

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

    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    if (_supportedExtensions.EXT_memory_budget)
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
#ifndef NDEBUG
    if (_supportedExtensions.AMD_device_coherent_memory)
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
#endif

    vmaCreateAllocator(&allocatorCreateInfo, &_allocator);

    //get depth format supported for swapchain
    for (VkFormat format : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        VkFormatProperties depthProperties;
        vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &depthProperties);

        if ((depthProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            _depthFormat = static_cast<Format>(format);
            break;
        }
    }

    //cache queues for later use
    u32 queueIndices[4];
    if (queueIndex(queueIndices[0], QueueType::GRAPHICS))
        vkGetDeviceQueue(_logicalDevice, queueIndices[0], 0, &_graphicsQueue);

    if (queueIndex(queueIndices[1], QueueType::COMPUTE, QueueType::GRAPHICS | QueueType::TRANSFER) ||
            queueIndex(queueIndices[1], QueueType::COMPUTE, QueueType::GRAPHICS))
        vkGetDeviceQueue(_logicalDevice, queueIndices[1], 0, &_computeQueue);

    if (queueIndex(queueIndices[2], QueueType::TRANSFER, QueueType::GRAPHICS | QueueType::COMPUTE) ||
            queueIndex(queueIndices[2], QueueType::TRANSFER, QueueType::GRAPHICS))
        vkGetDeviceQueue(_logicalDevice, queueIndices[2], 0, &_transferQueue);

    if (queueIndex(queueIndices[3], QueueType::PRESENT))
        vkGetDeviceQueue(_logicalDevice, queueIndices[3], 0, &_presentQueue);

    VkQueryPoolCreateInfo queryPoolCreateInfo{};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.queryCount = 20 * 2;
    _timestampQueryPool = VK_NULL_HANDLE;
    VK_TRY(vkCreateQueryPool(_logicalDevice, &queryPoolCreateInfo, nullptr, &_timestampQueryPool));


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
    VK_TRY(vkCreateQueryPool(_logicalDevice, &pipelineStatisticsCreate, nullptr, &_pipelineStatistics));

    vkResetQueryPool(_logicalDevice, _timestampQueryPool, 0, 10);
}

cala::backend::vulkan::Context::~Context() {
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

void cala::backend::vulkan::Context::beginDebugLabel(VkCommandBuffer buffer, std::string_view label, std::array<f32, 4> colour) const {
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

void cala::backend::vulkan::Context::endDebugLabel(VkCommandBuffer buffer) const {
#ifndef NDEBUG
//    static auto endDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(_instance, "vkCmdEndDebugUtilsLabelEXT");
//    endDebugLabel(buffer);
    vkCmdEndDebugUtilsLabelEXT(buffer);
#endif
}


void cala::backend::vulkan::Context::setDebugName(u32 type, u64 object, std::string_view name) {
#ifndef NDEBUG
//    if (_supportedExtensions.EXT_debug_marker) {
////        static auto setNameObject = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(_instance, "vkDebugMarkerSetObjectNameEXT");
//        VkDebugMarkerObjectNameInfoEXT nameInfo{};
//        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
//        nameInfo.objectType = static_cast<VkDebugReportObjectTypeEXT>(type);
//        nameInfo.object = object;
//        nameInfo.pObjectName = name.data();
////        setNameObject(_logicalDevice, &nameInfo);
//    vkDebugMarkerSetObjectNameEXT(_logicalDevice, &nameInfo);
//    }
#endif
}


bool cala::backend::vulkan::Context::queueIndex(u32& index, QueueType type, QueueType rejectType) const {
    u32 flags = static_cast<u32>(type);
    u32 rejected = static_cast<u32>(rejectType);
    if (flags & 0x20) {
        index = 0;
        return true;
    }

    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, nullptr);
    ende::Vector<VkQueueFamilyProperties> familyProperties(count);
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

VkQueue cala::backend::vulkan::Context::getQueue(QueueType type) const {
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

u32 cala::backend::vulkan::Context::memoryIndex(u32 filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((filter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    return 0; //TODO: error on none available
}


const char *cala::backend::vulkan::Context::deviceTypeString() const {
    switch (_deviceType) {
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

cala::backend::vulkan::Context::PipelineStatistics cala::backend::vulkan::Context::getPipelineStatistics() const {
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