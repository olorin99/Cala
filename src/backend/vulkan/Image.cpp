#include "Cala/backend/vulkan/Image.h"


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