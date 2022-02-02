#include "Cala/backend/vulkan/Image.h"
#include <Cala/backend/vulkan/Driver.h>

cala::backend::vulkan::Image::Image(Context& context, CreateInfo info)
    : _context(context),
    _image(VK_NULL_HANDLE),
    _memory(VK_NULL_HANDLE)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    if (info.depth > 1)
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
    else if (info.height > 1)
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
    else
        imageInfo.imageType = VK_IMAGE_TYPE_1D;
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

    vkCreateImage(_context._device, &imageInfo, nullptr, &_image);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(_context._device, _image, &memoryRequirements);
    _memory = _context.allocate(memoryRequirements.size, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkBindImageMemory(_context._device, _image, _memory, 0);

    _width = info.width;
    _height = info.height;
    _depth = info.depth;
    _format = info.format;
}

cala::backend::vulkan::Image::~Image() {
    vkDestroyImage(_context._device, _image, nullptr);
    vkFreeMemory(_context._device, _memory, nullptr);
}


cala::backend::vulkan::Image::View::~View() {
    vkDestroyImageView(_device, view, nullptr);
}


void cala::backend::vulkan::Image::data(cala::backend::vulkan::Driver& driver, DataInfo info) {

    auto staging = driver.stagingBuffer(info.width * info.height * info.depth * info.format);
    staging.data(info.data);
    driver.immediate([&](VkCommandBuffer buffer) {
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier imageBarrier{};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.image = _image;
        imageBarrier.subresourceRange = range;
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = info.mipLevel;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = info.width;
        copyRegion.imageExtent.height = info.height;
        copyRegion.imageExtent.depth = info.depth;

        vkCmdCopyBufferToImage(buffer, staging.buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

//        VkImageMemoryBarrier imageBarrier1{}
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    });
}


cala::backend::vulkan::Image::View cala::backend::vulkan::Image::getView(VkImageViewType viewType, VkImageAspectFlags aspect, u32 mipLevel, u32 levelCount, u32 arrayLayer, u32 layerCount) {
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    viewCreateInfo.image = _image;
    viewCreateInfo.viewType = viewType;
    viewCreateInfo.format = _format;

    viewCreateInfo.subresourceRange.aspectMask = aspect;
    viewCreateInfo.subresourceRange.baseMipLevel = mipLevel;
    viewCreateInfo.subresourceRange.levelCount = levelCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = arrayLayer;
    viewCreateInfo.subresourceRange.layerCount = layerCount;

    VkImageView view;
    vkCreateImageView(_context._device, &viewCreateInfo, nullptr, &view);
    View v{};
    v.view = view;
    v._device = _context._device;
    return v;
}