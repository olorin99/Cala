#define VMA_IMPLEMENTATION
#include <Cala/backend/vulkan/vk_mem_alloc.h>

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/primitives.h>

#include <Ende/Vector.h>
#include <Ende/log/log.h>
#include <cstring>
#include <iostream>

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

const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
        ) {
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        return VK_FALSE;
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            ende::log::info("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            ende::log::info("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            ende::log::warn("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            ende::log::error("{}", pCallbackData->pMessage);
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

cala::backend::vulkan::Context::Context(cala::backend::Platform& platform) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cala";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Cala";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    auto extensions = platform.requiredExtensions();
#ifndef NDEBUG
    extensions.push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

//    extensions.push(VK_EXT_validation_features);

//    VkValidationFeatureEnableEXT v[2] = {VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT};
////    auto v = VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT;
//
//    VkValidationFeaturesEXT validationFeatures{};
//    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
//    validationFeatures.pEnabledValidationFeatures = v;
//    validationFeatures.enabledValidationFeatureCount = 2;

    //create instance with required extensions
    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    instanceCreateInfo.enabledExtensionCount = extensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
//    instanceCreateInfo.pNext = &validationFeatures;

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &_instance) != VK_SUCCESS)
        throw VulkanContextException("Instance Creation Error");

    //create debug messenger
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    debugInfo.pUserData = nullptr;

    auto createDebugUtils = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
    createDebugUtils(_instance, &debugInfo, nullptr, &_debugMessenger);
#endif

    //get physical device
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw VulkanContextException("No GPUs found with vulkan support");

    ende::Vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

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


    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = 3;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
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
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;

    vulkan12Features.hostQueryReset = VK_TRUE;

    if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device) != VK_SUCCESS)
        throw VulkanContextException("Failed to create logical device");

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.vulkanApiVersion = appInfo.apiVersion;
    allocatorCreateInfo.physicalDevice = _physicalDevice;
    allocatorCreateInfo.device = _device;
    allocatorCreateInfo.instance = _instance;
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

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
    vkGetDeviceQueue(_device, queueIndex(QueueType::GRAPHICS), 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, queueIndex(QueueType::COMPUTE), 0, &_computeQueue);

    VkQueryPoolCreateInfo queryPoolCreateInfo{};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.queryCount = 20 * 2;
    _timestampQueryPool = VK_NULL_HANDLE;
    VkResult res = vkCreateQueryPool(_device, &queryPoolCreateInfo, nullptr, &_timestampQueryPool);
    if (res != VK_SUCCESS)
        throw "Unable to create query pool";


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
    vkCreateQueryPool(_device, &pipelineStatisticsCreate, nullptr, &_pipelineStatistics);

    vkResetQueryPool(_device, _timestampQueryPool, 0, 10);
}

cala::backend::vulkan::Context::~Context() {
    vkDestroyQueryPool(_device, _pipelineStatistics, nullptr);
    vkDestroyQueryPool(_device, _timestampQueryPool, nullptr);
    vmaDestroyAllocator(_allocator);
    vkDestroyDevice(_device, nullptr);

#ifndef NDEBUG
    auto destroyDebugUtils = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
    destroyDebugUtils(_instance, _debugMessenger, nullptr);
#endif

    vkDestroyInstance(_instance, nullptr);
}

void cala::backend::vulkan::Context::beginDebugLabel(VkCommandBuffer buffer, std::string_view label, std::array<f32, 4> colour) const {
#ifndef NDEBUG
    static auto beginDebugLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(_instance, "vkCmdBeginDebugUtilsLabelEXT");
    VkDebugUtilsLabelEXT labelExt{};
    labelExt.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelExt.pLabelName = label.data();
    for (i32 i = 0; i < 4; i++)
        labelExt.color[i] = colour[i];
    beginDebugLabel(buffer, &labelExt);
#endif
}

void cala::backend::vulkan::Context::endDebugLabel(VkCommandBuffer buffer) const {
#ifndef NDEBUG
    static auto endDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(_instance, "vkCmdEndDebugUtilsLabelEXT");
    endDebugLabel(buffer);
#endif
}



u32 cala::backend::vulkan::Context::queueIndex(QueueType type) const {
    u32 flags = static_cast<u32>(type);
    if (flags & 0x20)
        return 0;

    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, nullptr);
    ende::Vector<VkQueueFamilyProperties> familyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, familyProperties.data());

    u32 i = 0;
    for (auto& queueFamily : familyProperties) {
        if (queueFamily.queueCount > 0) {
            if (queueFamily.queueFlags & flags)
                return i;
        }
        i++;
    }
    return i;
}

VkQueue cala::backend::vulkan::Context::getQueue(QueueType type) const {
    switch (type) {
        case QueueType::GRAPHICS:
            return _graphicsQueue;
        case QueueType::COMPUTE:
            return _computeQueue;
    }
    VkQueue queue;
    vkGetDeviceQueue(_device, queueIndex(type), 0, &queue);
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


VkDeviceMemory cala::backend::vulkan::Context::allocate(u32 size, u32 typeBits, MemoryProperties flags) {
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = size;
    allocateInfo.memoryTypeIndex = memoryIndex(typeBits, getMemoryProperties(flags));

    VkDeviceMemory memory;
    vkAllocateMemory(_device, &allocateInfo, nullptr, &memory);
    return memory;
}


std::pair<VkBuffer, VkDeviceMemory> cala::backend::vulkan::Context::createBuffer(u32 size, u32 usage, MemoryProperties flags) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

    VkDeviceMemory memory = allocate(memRequirements.size, memRequirements.memoryTypeBits, flags);
    vkBindBufferMemory(_device, buffer, memory, 0);

    return {buffer, memory};
}

std::pair <VkImage, VkDeviceMemory> cala::backend::vulkan::Context::createImage(const CreateImage info) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = info.imageType;
    imageInfo.format = info.format;
    imageInfo.extent.width = info.width;
    imageInfo.extent.height = info.height;
    imageInfo.extent.depth = info.depth;
    imageInfo.mipLevels = info.mipLevels;
    imageInfo.arrayLayers = info.arrayLayers;

    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = info.usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VkImage image;
    vkCreateImage(_device, &imageInfo, nullptr, &image);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_device, image, &memRequirements);

    VkDeviceMemory memory = allocate(memRequirements.size, memRequirements.memoryTypeBits, MemoryProperties::DEVICE_LOCAL);

    vkBindImageMemory(_device, image, memory, 0);
    return {image, memory};
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
    vkGetQueryPoolResults(_device, _pipelineStatistics, 0, 1, 6 * sizeof(u64), (void*)_pipelineStats, sizeof(u64), VK_QUERY_RESULT_64_BIT);
    return {
        _pipelineStats[0],
        _pipelineStats[1],
        _pipelineStats[2],
        _pipelineStats[3],
        _pipelineStats[4],
        _pipelineStats[5]
    };
}