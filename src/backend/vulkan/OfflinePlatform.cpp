#include "Cala/backend/vulkan/OfflinePlatform.h"


cala::backend::vulkan::OfflinePlatform::OfflinePlatform(u32 width, u32 height, u32 flags)
    : _width(width),
    _height(height)
{}

std::vector<const char *> cala::backend::vulkan::OfflinePlatform::requiredExtensions() {
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    return extensions;
}

VkSurfaceKHR cala::backend::vulkan::OfflinePlatform::surface(VkInstance instance) {
    return VK_NULL_HANDLE;
}

std::pair<u32, u32> cala::backend::vulkan::OfflinePlatform::windowSize() const {
    return { _width, _height };
}