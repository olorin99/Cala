
#include <Cala/backend/vulkan/primitives.h>
#include "Cala/backend/vulkan/Swapchain.h"
#include <Cala/backend/vulkan/Device.h>
#include <limits>
#include <algorithm>


VkSurfaceCapabilitiesKHR getCapabilities(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    return capabilities;
}

VkSurfaceFormatKHR getSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface) {
    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.data());

    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        return {VK_FORMAT_B8G8R8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    for (auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }
    return formats.front();
}

VkPresentModeKHR getValidPresentMode(VkPhysicalDevice device, VkSurfaceKHR surface, VkPresentModeKHR preference = VK_PRESENT_MODE_MAILBOX_KHR) {
    u32 count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes.data());

    for (auto& mode : modes) {
        if (mode == preference)
            return mode;
    }
    for (auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }
    for (auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D getExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) {
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        return capabilities.currentExtent;

    VkExtent2D extent = {width, height};
    extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
    extent.height = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.height, extent.height));
    return extent;
}

cala::backend::vulkan::Swapchain::Swapchain(Device &driver, Platform& platform, bool clear)
    : _driver(driver),
    _swapchain(VK_NULL_HANDLE),
    _frame(0),
    _depthImage({}),
    _depthView(),
    _mode(PresentMode::MAILBOX)
{
    _surface = platform.surface(_driver.context().instance());
//    VkBool32 supported = VK_FALSE;
    u32 index = 0;
    _driver.context().queueIndex(index, QueueType::GRAPHICS);
//    vkGetPhysicalDeviceSurfaceSupportKHR(_device.context().physicalDevice(), index, _surface, &supported);
    auto windowSize = platform.windowSize();
    _extent = { windowSize.first, windowSize.second };

    //TODO: get better error handling
    if (!createSwapchain()) throw std::runtime_error("Unable to create swapchain");
    if (!createImageViews()) throw std::runtime_error("Unable to create swapchains image views");
    if (!createSemaphores()) throw std::runtime_error("Unable to create swapchains semaphores");

    _depthImage = driver.createImage({
            _extent.width, _extent.height, 1, driver.context().depthFormat(), 1, 1, backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT | ImageUsage::SAMPLED
    });
    _depthView = _depthImage->newView();

    std::array<RenderPass::Attachment, 2> attachments = {
            RenderPass::Attachment{
                    format(),
                    VK_SAMPLE_COUNT_1_BIT,
                    clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            RenderPass::Attachment{
                    _driver.context().depthFormat(),
                    VK_SAMPLE_COUNT_1_BIT,
                    clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
    };

    _renderPass = _driver.getRenderPass(attachments);

    for (auto& view : _imageViews) {
        VkImageView framebufferAttachments[2] = { view, _depthView.view };
        u32 hashes[2] = { 0, 0 };
        _framebuffers.emplace_back(_driver.getFramebuffer(_renderPass, framebufferAttachments, hashes, _extent.width, _extent.height));
    }
}

cala::backend::vulkan::Swapchain::~Swapchain() {
//    _driver.destroyImage(_depthImage);
    _depthImage.release();

    for (auto& view : _imageViews)
        vkDestroyImageView(_driver.context().device(), view, nullptr);

    if (_surface != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_driver.context().device(), _swapchain, nullptr);
        vkDestroySurfaceKHR(_driver.context().instance(), _surface, nullptr);
    } else {
        for (u32 i = 0; i < _images.size(); i++)
            vmaDestroyImage(_driver.context().allocator(), _images[i], _allocations[i]);
    }
}


std::expected<cala::backend::vulkan::Swapchain::Frame, i32> cala::backend::vulkan::Swapchain::nextImage() {
    auto semaphorePair = std::move(_semaphores.back());
    _semaphores.pop_back();
    u32 index = 0;
    if (_surface != VK_NULL_HANDLE) {
        VkResult result = vkAcquireNextImageKHR(_driver.context().device(), _swapchain, std::numeric_limits<u64>::max(), semaphorePair.acquire.semaphore(), VK_NULL_HANDLE, &index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            resize(_depthImage->width(), _depthImage->height());
        } else if (result == VK_ERROR_DEVICE_LOST)
            return std::unexpected((i32)VK_ERROR_DEVICE_LOST);
    } else {
        index = (index + 1) % 2;
    }
    return Frame{ _frame++, index, std::move(semaphorePair), _framebuffers[index] };
}

bool cala::backend::vulkan::Swapchain::present(Frame frame) {
    auto presentSemaphore = frame.semaphores.present.semaphore();
    _semaphores.push_back(std::move(frame.semaphores));
    std::rotate(_semaphores.begin(), _semaphores.end() - 1, _semaphores.end());

    if (_surface != VK_NULL_HANDLE) {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &presentSemaphore;

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &_swapchain;

        presentInfo.pImageIndices = &frame.index;
        presentInfo.pResults = nullptr;

        auto res = vkQueuePresentKHR(_driver.context().getQueue(QueueType::PRESENT), &presentInfo);
        if (res == VK_ERROR_DEVICE_LOST) {
            _driver.logger().error("Device lost on queue submit");
            _driver.printMarkers();
            throw std::runtime_error("Device lost on queue submit");
        }
        return res == VK_SUCCESS;
    }
    return false;
}

bool cala::backend::vulkan::Swapchain::resize(u32 width, u32 height) {
    _framebuffers.clear();
    for (auto& view : _imageViews)
        vkDestroyImageView(_driver.context().device(), view, nullptr);

    _extent = { width, height };

    createSwapchain();
    createImageViews();

//    _driver.destroyImage(_depthImage);
    _depthImage = _driver.createImage({ _extent.width, _extent.height, 1, _driver.context().depthFormat(), 1, 1, ImageUsage::DEPTH_STENCIL_ATTACHMENT | ImageUsage::SAMPLED });
    _depthView = _depthImage->newView();

    for (auto& view : _imageViews) {
        VkImageView framebufferAttachments[2] = { view, _depthView.view };
        u32 hashes[2] = { 0, 0 };
        _framebuffers.emplace_back(_driver.getFramebuffer(_renderPass, framebufferAttachments, hashes, _extent.width, _extent.height));
    }
    return true;
}

void cala::backend::vulkan::Swapchain::blitImageToFrame(u32 index, CommandBuffer &buffer, Image &src) {
    VkImageSubresourceRange range{};
    range.aspectMask = isDepthFormat(src.format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkImageMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = getPipelineStage(PipelineStage::TOP);
    memoryBarrier.dstStageMask = getPipelineStage(PipelineStage::TRANSFER);
    memoryBarrier.srcAccessMask = getAccessFlags(Access::NONE);
    memoryBarrier.dstAccessMask = getAccessFlags(Access::TRANSFER_WRITE);
    memoryBarrier.oldLayout = getImageLayout(ImageLayout::UNDEFINED);
    memoryBarrier.newLayout = getImageLayout(ImageLayout::TRANSFER_DST);
    memoryBarrier.image = _images[index];
    memoryBarrier.subresourceRange = range;

    buffer.pipelineBarrier({}, { &memoryBarrier, 1});

    VkImageSubresourceLayers srcSubresource{};
    srcSubresource.aspectMask = isDepthFormat(src.format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    srcSubresource.mipLevel = 0;
    srcSubresource.baseArrayLayer = 0;
    srcSubresource.layerCount = 1;

    VkImageSubresourceLayers dstSubresource{};
    dstSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    dstSubresource.mipLevel = 0;
    dstSubresource.baseArrayLayer = 0;
    dstSubresource.layerCount = 1;

    VkImageBlit blit{};
    blit.srcSubresource = srcSubresource;
    blit.srcOffsets[0] = {0, 0, 0 };
    blit.srcOffsets[1] = { static_cast<i32>(src.width()), static_cast<i32>(src.height()), static_cast<i32>(1) };
    blit.dstSubresource = dstSubresource;
    blit.dstOffsets[0] = {0, 0, 0 };
    blit.dstOffsets[1] = { static_cast<i32>(_extent.width), static_cast<i32>(_extent.height), static_cast<i32>(1) };

    vkCmdBlitImage(buffer.buffer(), src.image(), getImageLayout(ImageLayout::TRANSFER_SRC), _images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

    memoryBarrier.srcStageMask = getPipelineStage(PipelineStage::TRANSFER);
    memoryBarrier.dstStageMask = getPipelineStage(PipelineStage::BOTTOM);
    memoryBarrier.srcAccessMask = getAccessFlags(Access::TRANSFER_WRITE);
    memoryBarrier.dstAccessMask = getAccessFlags(Access::NONE);
    memoryBarrier.oldLayout = getImageLayout(ImageLayout::TRANSFER_DST);
    memoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    buffer.pipelineBarrier({}, { &memoryBarrier, 1});
}

void cala::backend::vulkan::Swapchain::copyImageToFrame(u32 index, CommandBuffer &buffer, Image &src) {
    assert(_extent.width == src.width() && _extent.height == src.height() && 1 == src.depth());

    auto barriers = src.barrier(PipelineStage::COMPUTE_SHADER | PipelineStage::VERTEX_SHADER | PipelineStage::FRAGMENT_SHADER, PipelineStage::TRANSFER, Access::SHADER_WRITE, Access::TRANSFER_WRITE, ImageLayout::COLOUR_ATTACHMENT, ImageLayout::TRANSFER_SRC);
    buffer.pipelineBarrier({ &barriers, 1 });


    VkImageSubresourceRange range{};
    range.aspectMask = (_format == Format::D16_UNORM || _format == Format::D32_SFLOAT || _format == Format::D24_UNORM_S8_UINT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkImageMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = getPipelineStage(PipelineStage::TOP);
    memoryBarrier.dstStageMask = getPipelineStage(PipelineStage::TRANSFER);
    memoryBarrier.srcAccessMask = getAccessFlags(Access::NONE);
    memoryBarrier.dstAccessMask = getAccessFlags(Access::TRANSFER_WRITE);
    memoryBarrier.oldLayout = getImageLayout(ImageLayout::UNDEFINED);
    memoryBarrier.newLayout = getImageLayout(ImageLayout::TRANSFER_DST);
    memoryBarrier.image = _images[index];
    memoryBarrier.subresourceRange = range;

    buffer.pipelineBarrier({}, { &memoryBarrier, 1});

    VkImageCopy region{};

    region.dstSubresource.aspectMask = _format == _driver.context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    region.srcSubresource.aspectMask = src.format() == _driver.context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;

    region.srcOffset = { 0, 0, 0 };
    region.dstOffset = { 0, 0, 0 };
    region.extent = { _extent.width, _extent.height, 1 };
    vkCmdCopyImage(buffer.buffer(), src.image(), getImageLayout(ImageLayout::TRANSFER_SRC), _images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    memoryBarrier.srcStageMask = getPipelineStage(PipelineStage::TRANSFER);
    memoryBarrier.dstStageMask = getPipelineStage(PipelineStage::BOTTOM);
    memoryBarrier.srcAccessMask = getAccessFlags(Access::TRANSFER_WRITE);
    memoryBarrier.dstAccessMask = getAccessFlags(Access::NONE);
    memoryBarrier.oldLayout = getImageLayout(ImageLayout::TRANSFER_DST);
    memoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    buffer.pipelineBarrier({}, { &memoryBarrier, 1});
}

void cala::backend::vulkan::Swapchain::copyFrameToImage(u32 index, cala::backend::vulkan::CommandBuffer &buffer, cala::backend::vulkan::Image &dst) {
    assert(_extent.width == dst.width() && _extent.height == dst.height() && 1 == dst.depth());

    auto barriers = dst.barrier(PipelineStage::TRANSFER, PipelineStage::TRANSFER, Access::TRANSFER_WRITE, Access::TRANSFER_WRITE, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST);
    buffer.pipelineBarrier({ &barriers, 1 });

    VkImageCopy region{};

    region.srcSubresource.aspectMask = _format == _driver.context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;

    region.dstSubresource.aspectMask = dst.format() == _driver.context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    region.srcOffset = { 0, 0, 0 };
    region.dstOffset = { 0, 0, 0 };
    region.extent = { _extent.width, _extent.height, 1 };
    vkCmdCopyImage(buffer.buffer(), _images[index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, dst.image(), getImageLayout(dst.layout()), 1, &region);


}

void cala::backend::vulkan::Swapchain::copyFrameToImage(u32 index, cala::backend::vulkan::Image &dst) {
    _driver.immediate([&](CommandBuffer& cmd) {
        copyFrameToImage(index, cmd, dst);
    });
}

void cala::backend::vulkan::Swapchain::setPresentMode(cala::backend::PresentMode mode) {
    _mode = mode;
    resize(_extent.width, _extent.height);
}


bool cala::backend::vulkan::Swapchain::createSwapchain() {
    if (_surface != VK_NULL_HANDLE) {
        VkSurfaceCapabilitiesKHR capabilities = getCapabilities(_driver.context().physicalDevice(), _surface);
        VkSurfaceFormatKHR format = getSurfaceFormat(_driver.context().physicalDevice(), _surface);
        VkPresentModeKHR mode = getValidPresentMode(_driver.context().physicalDevice(), _surface, backend::vulkan::getPresentMode(_mode));
        VkExtent2D extent = getExtent(capabilities, _extent.width, _extent.height);

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
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = mode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = _swapchain;

        VkSwapchainKHR oldSwap = _swapchain;

        VkResult result = vkCreateSwapchainKHR(_driver.context().device(), &createInfo, nullptr, &_swapchain);

        u32 count = 0;
        vkGetSwapchainImagesKHR(_driver.context().device(), _swapchain, &count, nullptr);
        _images.resize(count);
        vkGetSwapchainImagesKHR(_driver.context().device(), _swapchain, &count, _images.data());

        vkDestroySwapchainKHR(_driver.context().device(), oldSwap, nullptr);

        _extent = extent;
        _format = static_cast<Format>(format.format);
        return result == VK_SUCCESS;
    } else {
        u32 imageCount = 2;
        _images.resize(imageCount);
        _allocations.resize(imageCount);
        VkResult res = VK_SUCCESS;

        for (u32 i = 0; i < imageCount; i++) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

            imageInfo.imageType = VK_IMAGE_TYPE_2D;

            imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.extent.width = _extent.width;
            imageInfo.extent.height = _extent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;

            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = 0;

            res = vmaCreateImage(_driver.context().allocator(), &imageInfo, &allocInfo, &_images[i], &_allocations[i], nullptr);
            if (res != VK_SUCCESS)
                return false;
            _format = Format::RGBA8_UNORM;
        }
    }
    return true;
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

        if (vkCreateImageView(_driver.context().device(), &createInfo, nullptr, &_imageViews[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool cala::backend::vulkan::Swapchain::createSemaphores() {
    if (!_semaphores.empty())
        return false;

    if (_surface != VK_NULL_HANDLE) {
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (u32 i = 0; i < _imageViews.size(); i++) {
            _semaphores.push_back({{&_driver}, {&_driver}});
        }
    } else {
        _semaphores.push_back({{nullptr}, {nullptr}});
    }

    return true;
}