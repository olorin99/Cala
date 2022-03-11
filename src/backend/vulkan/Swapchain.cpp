#include <Ende/Vector.h>
#include <Cala/backend/vulkan/primitives.h>
#include "Cala/backend/vulkan/Swapchain.h"


VkSurfaceCapabilitiesKHR getCapabilities(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    return capabilities;
}

VkSurfaceFormatKHR getSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface) {
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

cala::backend::vulkan::Swapchain::Swapchain(Context &context, Platform& platform)
    : _context(context),
    _swapchain(VK_NULL_HANDLE),
    _frame(0),
    _depthImage(context, {
        800, 600, 1, _context.depthFormat(), 1, 1, backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT
    }),
    _depthView(_depthImage.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT))
{
    _surface = platform.surface(context.instance());
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice(), context.queueIndex(VK_QUEUE_GRAPHICS_BIT), _surface, &supported);

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(_context.device(), &fenceCreateInfo, nullptr, &_fence);

    //TODO: get better error handling
    if (!createSwapchain()) throw "Unable to create swapchain";
    if (!createImageViews()) throw "Unable to create swapchains image views";
    if (!createSemaphores()) throw "Unable to create swapchains semaphores";

    std::array<RenderPass::Attachment, 2> attachments = {
            RenderPass::Attachment{
                    format(),
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            RenderPass::Attachment{
                    _context.depthFormat(),
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
    };

    _renderPass = new RenderPass(_context, attachments);

    for (auto& view : _imageViews) {
        VkImageView framebufferAttachments[2] = { view, _depthView.view };
        _framebuffers.emplace(_context.device(), *_renderPass, framebufferAttachments, 800, 600);
    }
}

cala::backend::vulkan::Swapchain::~Swapchain() {
    delete _renderPass;

    for (auto& semaphore : _semaphores) {
        vkDestroySemaphore(_context.device(), semaphore, nullptr);
    }

    for (auto& view : _imageViews)
        vkDestroyImageView(_context.device(), view, nullptr);

    vkDestroyFence(_context.device(), _fence, nullptr);
    vkDestroySwapchainKHR(_context.device(), _swapchain, nullptr);
    vkDestroySurfaceKHR(_context.instance(), _surface, nullptr);
}


cala::backend::vulkan::Swapchain::Frame cala::backend::vulkan::Swapchain::nextImage() {
    auto image = _semaphores.pop().unwrap();
    u32 index = 0;
    vkAcquireNextImageKHR(_context.device(), _swapchain, std::numeric_limits<u64>::max(), image, VK_NULL_HANDLE, &index);
    return { _frame++, index, image, _fence, _framebuffers[index] };
}

bool cala::backend::vulkan::Swapchain::present(Frame frame, VkSemaphore renderFinish) {
    _semaphores.push(frame.imageAquired);
    std::rotate(_semaphores.begin(), _semaphores.end() - 1, _semaphores.end());

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinish;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &_swapchain;

    presentInfo.pImageIndices = &frame.index;
    presentInfo.pResults = nullptr;

    auto res = vkQueuePresentKHR(_context.getQueue(0x20), &presentInfo) == VK_SUCCESS;

    return res;
}

bool cala::backend::vulkan::Swapchain::wait(u64 timeout) {
    auto res = vkWaitForFences(_context.device(), 1, &_fence, true, timeout) == VK_SUCCESS;
    if (res)
        vkResetFences(_context.device(), 1, &_fence);
    return res;
}


bool cala::backend::vulkan::Swapchain::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities = getCapabilities(_context.physicalDevice(), _surface);
    VkSurfaceFormatKHR format = getSurfaceFormat(_context.physicalDevice(), _surface);
    VkPresentModeKHR mode = getPresentMode(_context.physicalDevice(), _surface);
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

    VkResult result = vkCreateSwapchainKHR(_context.device(), &createInfo, nullptr, &_swapchain);

    u32 count = 0;
    vkGetSwapchainImagesKHR(_context.device(), _swapchain, &count, nullptr);
    _images.resize(count);
    vkGetSwapchainImagesKHR(_context.device(), _swapchain, &count, _images.data());

    _format = static_cast<Format>(format.format);
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
        createInfo.format = getFormat(_format);

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(_context.device(), &createInfo, nullptr, &_imageViews[i]) != VK_SUCCESS)
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

        if (vkCreateSemaphore(_context.device(), &createInfo, nullptr, &imageAcquired) != VK_SUCCESS)
            return false;

        _semaphores.push(imageAcquired);
    }
    return true;
}