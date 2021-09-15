#include "Cala/backend/vulkan/Context.h"

#include <Ende/Vector.h>

const char* validationLayers[] = {
        "VK_LAYER_KHRONOS_validation"
};

const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

bool checkDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    return deviceFeatures.geometryShader;
}

cala::backend::vulkan::Context::Context(ende::Span<const char *> extensions) {

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cala";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Cala";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    instanceCreateInfo.enabledExtensionCount = extensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &_instance) != VK_SUCCESS)
        throw "Instance Creation Error";


    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw "No GPUs found with vulkan support";

    ende::Vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

    for (auto& device : devices) {
        if (checkDeviceSuitable(device))
            _physicalDevice = device;
    }

    if (_physicalDevice == VK_NULL_HANDLE)
        throw "No suitable GPU found";

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

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;

    if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device) != VK_SUCCESS)
        throw "Failed to create logical device";


    _commands = new CommandBufferList(_device, queueIndex(VK_QUEUE_GRAPHICS_BIT));
    _pipelines = new PipelineList(_device);
}

cala::backend::vulkan::Context::~Context() {
    delete _pipelines;
    delete _commands;
    vkDestroyDevice(_device, nullptr);
    vkDestroyInstance(_instance, nullptr);
}


u32 cala::backend::vulkan::Context::queueIndex(u32 flags) {
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

VkQueue cala::backend::vulkan::Context::getQueue(u32 flags) {
    VkQueue queue;
    vkGetDeviceQueue(_device, queueIndex(flags), 0, &queue);
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


std::pair<VkBuffer, VkDeviceMemory> cala::backend::vulkan::Context::createBuffer(u32 size, u32 usage, u32 flags) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memRequirements.size;
    allocateInfo.memoryTypeIndex = memoryIndex(memRequirements.memoryTypeBits, flags);

    VkDeviceMemory memory;
    vkAllocateMemory(_device, &allocateInfo, nullptr, &memory);

    vkBindBufferMemory(_device, buffer, memory, 0);

    return {buffer, memory};
}