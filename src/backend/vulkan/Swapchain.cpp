#include <Ende/Vector.h>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include "Cala/backend/vulkan/Swapchain.h"

VkSurfaceKHR createSurface(cala::backend::vulkan::Context& context, void* window, void* display) {
    VkSurfaceKHR surface;
    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.window = *static_cast<Window*>(window);
    createInfo.dpy = static_cast<Display*>(display);
    vkCreateXlibSurfaceKHR(context._instance, &createInfo, nullptr, &surface);
    VkBool32 supported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(context._physicalDevice, context.queueIndex(VK_QUEUE_GRAPHICS_BIT), surface, &supported);
    return surface;
}

VkSurfaceCapabilitiesKHR getCapabilities(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    return capabilities;
}

VkSurfaceFormatKHR getFormat(VkPhysicalDevice device, VkSurfaceKHR surface) {
    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    ende::Vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.data());

    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        return {VK_FORMAT_B8G8R8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    for (auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }
    return formats.front();
}

VkPresentModeKHR getPresentMode(VkPhysicalDevice device, VkSurfaceKHR surface) {
    u32 count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    ende::Vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes.data());

    for (auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D getExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        return capabilities.currentExtent;

    VkExtent2D extent = {800, 600};
    extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
    extent.height = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.height, extent.height));
    return extent;
}

cala::backend::vulkan::Swapchain::Swapchain(Context &context, void *window, void *display)
    : _context(context),
    _swapchain(VK_NULL_HANDLE),
    _frame(0)
{
    _surface = createSurface(_context, window, display);

    //TODO: get better error handling
    if (!createSwapchain()) throw "Unable to create swapchain";
    if (!createImageViews()) throw "Unable to create swapchains image views";
    if (!createSemaphores()) throw "Unable to create swapchains semaphores";
}

cala::backend::vulkan::Swapchain::~Swapchain() {
    for (auto& semaphorePair : _semaphores) {
        vkDestroySemaphore(_context._device, semaphorePair.first, nullptr);
        vkDestroySemaphore(_context._device, semaphorePair.second, nullptr);
    }

    for (auto& view : _imageViews)
        vkDestroyImageView(_context._device, view, nullptr);

    vkDestroySwapchainKHR(_context._device, _swapchain, nullptr);
    vkDestroySurfaceKHR(_context._instance, _surface, nullptr);
}


cala::backend::vulkan::Swapchain::Frame cala::backend::vulkan::Swapchain::nextImage() {
//    auto [image, render] = _semaphores[_frame % 2];
    auto [image, render] = _semaphores.pop().unwrap();
    u32 index = 0;
    vkAcquireNextImageKHR(_context._device, _swapchain, std::numeric_limits<u64>::max(), image, VK_NULL_HANDLE, &index);
    return {_frame++, index, image, render};
}

bool cala::backend::vulkan::Swapchain::present(Frame frame) {

    _semaphores.push({frame.imageAquired, frame.renderFinished});
    std::rotate(_semaphores.begin(), _semaphores.end() - 1, _semaphores.end());

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinished;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &_swapchain;

    presentInfo.pImageIndices = &frame.index;
    presentInfo.pResults = nullptr;

    auto res = vkQueuePresentKHR(_context.getQueue(0x20), &presentInfo) == VK_SUCCESS;


    return res;
}


bool cala::backend::vulkan::Swapchain::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities = getCapabilities(_context._physicalDevice, _surface);
    VkSurfaceFormatKHR format = getFormat(_context._physicalDevice, _surface);
    VkPresentModeKHR mode = getPresentMode(_context._physicalDevice, _surface);
    VkExtent2D extent = getExtent(capabilities);

    u32 imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = _swapchain;

    VkResult result = vkCreateSwapchainKHR(_context._device, &createInfo, nullptr, &_swapchain);

    u32 count = 0;
    vkGetSwapchainImagesKHR(_context._device, _swapchain, &count, nullptr);
    _images.resize(count);
    vkGetSwapchainImagesKHR(_context._device, _swapchain, &count, _images.data());

    _format = format;
    _extent = extent;
    return result == VK_SUCCESS;
}

bool cala::backend::vulkan::Swapchain::createImageViews() {
    _imageViews.resize(_images.size());
    for (u32 i = 0; i < _imageViews.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        createInfo.image = _images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = _format.format;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(_context._device, &createInfo, nullptr, &_imageViews[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool cala::backend::vulkan::Swapchain::createSemaphores() {
    if (!_semaphores.empty())
        return false;

    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (u32 i = 0; i < _imageViews.size(); i++) {
        VkSemaphore imageAcquired;
        VkSemaphore renderFinished;

        if (vkCreateSemaphore(_context._device, &createInfo, nullptr, &imageAcquired) != VK_SUCCESS ||
                vkCreateSemaphore(_context._device, &createInfo, nullptr, &renderFinished) != VK_SUCCESS)
            return false;

        _semaphores.push({imageAcquired, renderFinished});
    }
    return true;
}