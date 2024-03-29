
#include <Cala/vulkan/primitives.h>
#include <Cala/vulkan/Swapchain.h>
#include <Cala/vulkan/Device.h>
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

std::expected<cala::vk::Swapchain, cala::vk::Error> cala::vk::Swapchain::create(cala::vk::Device* device, cala::vk::Swapchain::Info info) {
    Swapchain swapchain = {};

    if (!info.platform || !device)
        return std::unexpected(Error::INVALID_PLATFORM);

    swapchain._device = device;

    swapchain._surface = info.platform->surface(device->context().instance());
    auto windowSize = info.platform->windowSize();
    swapchain._extent = { windowSize.first, windowSize.second };

    auto createSwapchainResult = swapchain.createSwapchain();
    if (!createSwapchainResult)
        return std::unexpected(createSwapchainResult.error());
    auto createImageViewsResult = swapchain.createImageViews();
    if (!createImageViewsResult)
        return std::unexpected(Error::INVALID_SWAPCHAIN);
    if (!swapchain.createSemaphores())
        return std::unexpected(Error::INVALID_SWAPCHAIN);

    return swapchain;
}

cala::vk::Swapchain::~Swapchain() {
    if (!_device)
        return;

    for (auto& view : _imageViews)
        vkDestroyImageView(_device->context().device(), view, nullptr);

    if (_surface != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device->context().device(), _swapchain, nullptr);
        vkDestroySurfaceKHR(_device->context().instance(), _surface, nullptr);
    } else {
        for (u32 i = 0; i < _images.size(); i++)
            vmaDestroyImage(_device->context().allocator(), _images[i], _allocations[i]);
    }
}

cala::vk::Swapchain::Swapchain(cala::vk::Swapchain &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_surface, rhs._surface);
    std::swap(_swapchain, rhs._swapchain);
    std::swap(_format, rhs._format);
    std::swap(_mode, rhs._mode);
    std::swap(_extent, rhs._extent);
    std::swap(_frame, rhs._frame);
    std::swap(_images, rhs._images);
    std::swap(_imageViews, rhs._imageViews);
    std::swap(_semaphores, rhs._semaphores);
    std::swap(_allocations, rhs._allocations);
}

cala::vk::Swapchain &cala::vk::Swapchain::operator==(cala::vk::Swapchain &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_surface, rhs._surface);
    std::swap(_swapchain, rhs._swapchain);
    std::swap(_format, rhs._format);
    std::swap(_mode, rhs._mode);
    std::swap(_extent, rhs._extent);
    std::swap(_frame, rhs._frame);
    std::swap(_images, rhs._images);
    std::swap(_imageViews, rhs._imageViews);
    std::swap(_semaphores, rhs._semaphores);
    std::swap(_allocations, rhs._allocations);
    return *this;
}


std::expected<cala::vk::Swapchain::Frame, cala::vk::Error> cala::vk::Swapchain::nextImage() {
    auto semaphorePair = std::move(_semaphores.back());
    _semaphores.pop_back();
    u32 index = 0;
    if (_surface != VK_NULL_HANDLE) {
        VkResult result = vkAcquireNextImageKHR(_device->context().device(), _swapchain, std::numeric_limits<u64>::max(), semaphorePair.acquire.semaphore(), VK_NULL_HANDLE, &index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            resize(_extent.width, _extent.height);
        } else if (result == VK_ERROR_DEVICE_LOST)
            return std::unexpected(static_cast<Error>(VK_ERROR_DEVICE_LOST));
    } else {
        index = (index + 1) % 2;
    }
    return Frame{ _frame++, index, std::move(semaphorePair) };
}

std::expected<bool, cala::vk::Error> cala::vk::Swapchain::present(Frame frame) {
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

        auto res = vkQueuePresentKHR(_device->context().getQueue(QueueType::PRESENT), &presentInfo);
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            return std::unexpected(static_cast<Error>(res));
        }
        return res == VK_SUCCESS;
    }
    return std::unexpected(Error::INVALID_SURFACE);
}

bool cala::vk::Swapchain::resize(u32 width, u32 height) {
    for (auto& view : _imageViews)
        vkDestroyImageView(_device->context().device(), view, nullptr);

    _extent = { width, height };

    createSwapchain();
    createImageViews();
    return true;
}

void cala::vk::Swapchain::blitImageToFrame(u32 index, CommandHandle buffer, Image &src) {
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

    buffer->pipelineBarrier({}, { &memoryBarrier, 1});

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

    vkCmdBlitImage(buffer->buffer(), src.image(), getImageLayout(ImageLayout::TRANSFER_SRC), _images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

    memoryBarrier.srcStageMask = getPipelineStage(PipelineStage::TRANSFER);
    memoryBarrier.dstStageMask = getPipelineStage(PipelineStage::BOTTOM);
    memoryBarrier.srcAccessMask = getAccessFlags(Access::TRANSFER_WRITE);
    memoryBarrier.dstAccessMask = getAccessFlags(Access::NONE);
    memoryBarrier.oldLayout = getImageLayout(ImageLayout::TRANSFER_DST);
    memoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    buffer->pipelineBarrier({}, { &memoryBarrier, 1});
}

void cala::vk::Swapchain::copyImageToFrame(u32 index, CommandHandle buffer, Image &src) {
    assert(_extent.width == src.width() && _extent.height == src.height() && 1 == src.depth());

    auto barriers = src.barrier(PipelineStage::COMPUTE_SHADER | PipelineStage::VERTEX_SHADER | PipelineStage::FRAGMENT_SHADER, PipelineStage::TRANSFER, Access::SHADER_WRITE, Access::TRANSFER_WRITE, ImageLayout::COLOUR_ATTACHMENT, ImageLayout::TRANSFER_SRC);
    buffer->pipelineBarrier({ &barriers, 1 });


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

    buffer->pipelineBarrier({}, { &memoryBarrier, 1});

    VkImageCopy region{};

    region.dstSubresource.aspectMask = _format == _device->context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    region.srcSubresource.aspectMask = src.format() == _device->context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;

    region.srcOffset = { 0, 0, 0 };
    region.dstOffset = { 0, 0, 0 };
    region.extent = { _extent.width, _extent.height, 1 };
    vkCmdCopyImage(buffer->buffer(), src.image(), getImageLayout(ImageLayout::TRANSFER_SRC), _images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    memoryBarrier.srcStageMask = getPipelineStage(PipelineStage::TRANSFER);
    memoryBarrier.dstStageMask = getPipelineStage(PipelineStage::BOTTOM);
    memoryBarrier.srcAccessMask = getAccessFlags(Access::TRANSFER_WRITE);
    memoryBarrier.dstAccessMask = getAccessFlags(Access::NONE);
    memoryBarrier.oldLayout = getImageLayout(ImageLayout::TRANSFER_DST);
    memoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    buffer->pipelineBarrier({}, { &memoryBarrier, 1});
}

void cala::vk::Swapchain::copyFrameToImage(u32 index, cala::vk::CommandHandle buffer, cala::vk::Image &dst) {
    assert(_extent.width == dst.width() && _extent.height == dst.height() && 1 == dst.depth());

    auto barriers = dst.barrier(PipelineStage::TRANSFER, PipelineStage::TRANSFER, Access::TRANSFER_WRITE, Access::TRANSFER_WRITE, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST);
    buffer->pipelineBarrier({ &barriers, 1 });

    VkImageCopy region{};

    region.srcSubresource.aspectMask = _format == _device->context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;

    region.dstSubresource.aspectMask = dst.format() == _device->context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    region.srcOffset = { 0, 0, 0 };
    region.dstOffset = { 0, 0, 0 };
    region.extent = { _extent.width, _extent.height, 1 };
    vkCmdCopyImage(buffer->buffer(), _images[index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, dst.image(), getImageLayout(dst.layout()), 1, &region);


}

void cala::vk::Swapchain::copyFrameToImage(u32 index, cala::vk::Image &dst) {
    _device->immediate([&](CommandHandle cmd) {
        copyFrameToImage(index, cmd, dst);
    });
}

void cala::vk::Swapchain::setPresentMode(PresentMode mode) {
    _mode = mode;
    resize(_extent.width, _extent.height);
}


std::expected<void, cala::vk::Error> cala::vk::Swapchain::createSwapchain() {
    if (_surface != VK_NULL_HANDLE) {
        VkSurfaceCapabilitiesKHR capabilities = getCapabilities(_device->context().physicalDevice(), _surface);
        VkSurfaceFormatKHR format = getSurfaceFormat(_device->context().physicalDevice(), _surface);
        VkPresentModeKHR mode = getValidPresentMode(_device->context().physicalDevice(), _surface, vk::getPresentMode(_mode));
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

        VkResult result = vkCreateSwapchainKHR(_device->context().device(), &createInfo, nullptr, &_swapchain);
        if (result != VK_SUCCESS)
            return std::unexpected(static_cast<Error>(result));

        u32 count = 0;
        vkGetSwapchainImagesKHR(_device->context().device(), _swapchain, &count, nullptr);
        _images.resize(count);
        vkGetSwapchainImagesKHR(_device->context().device(), _swapchain, &count, _images.data());

        vkDestroySwapchainKHR(_device->context().device(), oldSwap, nullptr);

        _extent = extent;
        _format = static_cast<Format>(format.format);
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

            res = vmaCreateImage(_device->context().allocator(), &imageInfo, &allocInfo, &_images[i], &_allocations[i], nullptr);
            if (res != VK_SUCCESS)
                return std::unexpected(static_cast<Error>(res));
            _format = Format::RGBA8_UNORM;
        }
    }
    return {};
}

std::expected<void, cala::vk::Error> cala::vk::Swapchain::createImageViews() {
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

        auto result = vkCreateImageView(_device->context().device(), &createInfo, nullptr, &_imageViews[i]);
        if (result != VK_SUCCESS)
            return std::unexpected(static_cast<Error>(result));
    }
    return {};
}

bool cala::vk::Swapchain::createSemaphores() {
    if (!_semaphores.empty())
        return false;

    if (_surface != VK_NULL_HANDLE) {
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (u32 i = 0; i < _imageViews.size(); i++) {
            _semaphores.push_back({{_device}, {_device}});
        }
    } else {
        _semaphores.push_back({{nullptr}, {nullptr}});
    }

    return true;
}