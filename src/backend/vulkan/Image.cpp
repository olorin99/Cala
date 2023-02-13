#include "Cala/backend/vulkan/Image.h"
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/backend/vulkan/primitives.h>

cala::backend::vulkan::Image::Image(Driver& driver, CreateInfo info)
    : _driver(driver),
    _image(VK_NULL_HANDLE),
    _allocation(nullptr),
    _layout(info.initialLayout)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    _type = info.type;
    if (info.type == ImageType::AUTO) {
        if (info.depth > 1) {
            imageInfo.imageType = VK_IMAGE_TYPE_3D;
            _type = ImageType::IMAGE3D;
        }
        else if (info.height > 1) {
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            _type = ImageType::IMAGE2D;
        }
        else {
            imageInfo.imageType = VK_IMAGE_TYPE_1D;
            _type = ImageType::IMAGE1D;
        }
    } else {
        imageInfo.imageType = getImageType(info.type);
    }
    imageInfo.format = getFormat(info.format);
    imageInfo.extent.width = info.width;
    imageInfo.extent.height = info.height;
    imageInfo.extent.depth = info.depth;
    imageInfo.mipLevels = info.mipLevels;
    imageInfo.arrayLayers = info.arrayLayers;

    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = getImageUsage(info.usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (info.aliasFormat != Format::UNDEFINED) {
        imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        VkImageFormatListCreateInfo listCreateInfo{};
        listCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
        listCreateInfo.viewFormatCount = 1;
        VkFormat aliasFormat = getFormat(info.aliasFormat);
        listCreateInfo.pViewFormats = &aliasFormat;
        imageInfo.pNext = &listCreateInfo;
    }

    if (imageInfo.arrayLayers == 6)
        imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = 0;

    vmaCreateImage(driver.context().allocator(), &imageInfo, &allocInfo, &_image, &_allocation, nullptr);

//    driver.immediate([&](CommandBuffer& cmd) {
//        auto b = barrier(Access::NONE, Access::NONE, ImageLayout::UNDEFINED, info.initialLayout);
//        cmd.pipelineBarrier(PipelineStage::TOP, PipelineStage::TOP, 0, nullptr, { &b, 1 });
//    });

    _width = info.width;
    _height = info.height;
    _depth = info.depth;
    _layers = info.arrayLayers;
    _mips = info.mipLevels;
    _format = info.format;
    _usage = info.usage;
}

cala::backend::vulkan::Image::~Image() {
    if (_image != VK_NULL_HANDLE)
        vmaDestroyImage(_driver.context().allocator(), _image, _allocation);
}

cala::backend::vulkan::Image::Image(Image &&rhs) noexcept
    : _driver(rhs._driver),
    _image(VK_NULL_HANDLE),
    _allocation(nullptr),
    _width(0),
    _height(0),
    _depth(0),
    _format(Format::UNDEFINED),
    _usage(ImageUsage::COLOUR_ATTACHMENT)
{
    std::swap(_image, rhs._image);
    std::swap(_allocation, rhs._allocation);
    std::swap(_width, rhs._width);
    std::swap(_height, rhs._height);
    std::swap(_depth, rhs._depth);
    std::swap(_format, rhs._format);
    std::swap(_usage, rhs._usage);
}

cala::backend::vulkan::Image &cala::backend::vulkan::Image::operator=(Image &&rhs) noexcept {
    assert(&_driver == &rhs._driver);
    std::swap(_image, rhs._image);
    std::swap(_allocation, rhs._allocation);
    std::swap(_width, rhs._width);
    std::swap(_height, rhs._height);
    std::swap(_depth, rhs._depth);
    std::swap(_format, rhs._format);
    std::swap(_usage, rhs._usage);
    return *this;
}

cala::backend::vulkan::Image::View::View()
    : _device(VK_NULL_HANDLE),
    view(VK_NULL_HANDLE),
    _image(nullptr)
{}

cala::backend::vulkan::Image::View::~View() {
    if (_device == VK_NULL_HANDLE || view == VK_NULL_HANDLE || _image == nullptr) return;
    vkDestroyImageView(_device, view, nullptr);
}

cala::backend::vulkan::Image::View::View(View &&rhs) noexcept
    : _device(VK_NULL_HANDLE),
    view(VK_NULL_HANDLE),
    _image(nullptr)
{
    std::swap(_device, rhs._device);
    std::swap(view, rhs.view);
    std::swap(_image, rhs._image);
}

cala::backend::vulkan::Image::View &cala::backend::vulkan::Image::View::operator=(View &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(view, rhs.view);
    std::swap(_image, rhs._image);
    return *this;
}

void cala::backend::vulkan::Image::data(cala::backend::vulkan::Driver& driver, DataInfo info) {

    auto staging = driver.stagingBuffer(info.width * info.height * info.depth * info.format);
    staging.data(info.data);
    driver.immediate([&](CommandBuffer& buffer) {
        VkImageSubresourceRange range;
        range.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = info.layer;
        range.layerCount = 1;

        VkImageMemoryBarrier imageBarrier{};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.image = _image;
        imageBarrier.subresourceRange = range;
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(buffer.buffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = info.mipLevel;
        copyRegion.imageSubresource.baseArrayLayer = info.layer;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = info.width;
        copyRegion.imageExtent.height = info.height;
        copyRegion.imageExtent.depth = info.depth;

        vkCmdCopyBufferToImage(buffer.buffer(), staging.buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

//        VkImageMemoryBarrier imageBarrier1{}
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(buffer.buffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    });

    _layout = ImageLayout::SHADER_READ_ONLY;
}

void *cala::backend::vulkan::Image::map(u32 format) {
    void* address = nullptr;
    vmaMapMemory(_driver.context().allocator(), _allocation, &address);
    return address;
}

void cala::backend::vulkan::Image::unmap() {
    vmaUnmapMemory(_driver.context().allocator(), _allocation);
}

void cala::backend::vulkan::Image::blit(CommandBuffer &buffer, Image &dst, ImageLayout srcLayout, ImageLayout dstLayout, VkFilter filter) {
    VkImageSubresourceLayers srcSubresource{};
    srcSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    srcSubresource.mipLevel = 0;
    srcSubresource.baseArrayLayer = 0;
    srcSubresource.layerCount = _layers;

    VkImageSubresourceLayers dstSubresource{};
    dstSubresource.aspectMask = isDepthFormat(dst._format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    dstSubresource.mipLevel = 0;
    dstSubresource.baseArrayLayer = 0;
    dstSubresource.layerCount = dst._layers;

    VkImageBlit blit{};
    blit.srcSubresource = srcSubresource;
    blit.srcOffsets[0] = {0, 0, 0 };
    blit.srcOffsets[1] = { static_cast<i32>(_width), static_cast<i32>(_height), static_cast<i32>(_depth) };
    blit.dstSubresource = dstSubresource;
    blit.dstOffsets[0] = {0, 0, 0 };
    blit.dstOffsets[1] = { static_cast<i32>(dst._width), static_cast<i32>(dst._height), static_cast<i32>(dst._depth) };

    vkCmdBlitImage(buffer.buffer(), _image, getImageLayout(srcLayout), dst._image, getImageLayout(dstLayout), 1, &blit, filter);
}

void cala::backend::vulkan::Image::copy(cala::backend::vulkan::CommandBuffer &buffer, cala::backend::vulkan::Image &dst, u32 srcLayer, u32 dstLayer, u32 srcMipLevel, u32 dstMipLevel) {
    assert(_width == dst._width && _height == dst._height && _depth == dst._depth);

    VkImageCopy region{};

    region.srcSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = srcMipLevel;
    region.srcSubresource.baseArrayLayer = srcLayer;
    region.srcSubresource.layerCount = 1;

    region.dstSubresource.aspectMask = isDepthFormat(dst._format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = dstMipLevel;
    region.dstSubresource.baseArrayLayer = dstLayer;
    region.dstSubresource.layerCount = 1;

    region.srcOffset = { 0, 0, 0 };
    region.dstOffset = { 0, 0, 0 };
    region.extent = { _width, _height, _depth };
    vkCmdCopyImage(buffer.buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void cala::backend::vulkan::Image::generateMips() {
    _driver.immediate([&](CommandBuffer& cmd) {
        generateMips(cmd);
    });
}

void cala::backend::vulkan::Image::generateMips(CommandBuffer &cmd) {
    i32 mipWidth = _width;
    i32 mipHeight = _height;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = _image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    for (u32 layer = 0; layer < _layers; layer++) {
        barrier.subresourceRange.baseArrayLayer = layer;

        for (u32 mip = 1; mip < _mips; mip++) {
            barrier.subresourceRange.baseMipLevel = mip - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            cmd.pipelineBarrier(PipelineStage::TRANSFER, PipelineStage::TRANSFER, 0, nullptr, { &barrier, 1});

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = mip - 1;
            blit.srcSubresource.baseArrayLayer = layer;
            blit.srcSubresource.layerCount = 1;

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth, mipHeight, 1};
            blit.dstSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = mip;
            blit.dstSubresource.baseArrayLayer = layer;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd.buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            cmd.pipelineBarrier(PipelineStage::TRANSFER, PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &barrier, 1});
        }
        barrier.subresourceRange.baseMipLevel = _mips - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        cmd.pipelineBarrier(PipelineStage::TRANSFER, PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &barrier, 1});
    }
}


cala::backend::vulkan::Image::View cala::backend::vulkan::Image::newView(u32 mipLevel, u32 levelCount, u32 arrayLayer, u32 layerCount, cala::backend::ImageViewType viewType, Format format) {
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    if (viewType == ImageViewType::AUTO) {
        switch (_type) {
            case ImageType::IMAGE1D:
                if (_layers > 1)
                    viewType = ImageViewType::VIEW1D_ARRAY;
                else
                    viewType = ImageViewType::VIEW1D;
                break;
            case ImageType::IMAGE2D:
                if (_layers == 6)
                    viewType = ImageViewType::CUBE;
                else if (_layers > 1)
                    viewType = ImageViewType::VIEW2D_ARRAY;
                else
                    viewType = ImageViewType::VIEW2D;
                break;
            case ImageType::IMAGE3D:
                viewType = ImageViewType::VIEW3D;
                break;
            default:
                break;
        }

    }

    viewCreateInfo.image = _image;
    viewCreateInfo.viewType = getImageViewType(viewType);
    viewCreateInfo.format = format == Format::UNDEFINED ? getFormat(_format) : getFormat(format);

    viewCreateInfo.subresourceRange.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
//    viewCreateInfo.subresourceRange.aspectMask = _format == _driver.context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = mipLevel;
    viewCreateInfo.subresourceRange.levelCount = levelCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = arrayLayer;
    viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkImageView view;
    vkCreateImageView(_driver.context().device(), &viewCreateInfo, nullptr, &view);
    View v{};
    v.view = view;
    v._device = _driver.context().device();
    v._image = this;
    return v;
}

VkImageMemoryBarrier cala::backend::vulkan::Image::barrier(Access srcAccess, Access dstAccess, ImageLayout srcLayout, ImageLayout dstLayout, u32 layer) {
    VkImageSubresourceRange range{};
    range.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = layer;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = getAccessFlags(srcAccess);
    memoryBarrier.dstAccessMask = getAccessFlags(dstAccess);
    memoryBarrier.oldLayout = getImageLayout(srcLayout);
    memoryBarrier.newLayout = getImageLayout(dstLayout);
    memoryBarrier.image = _image;
    memoryBarrier.subresourceRange = range;
    return memoryBarrier;
}

void cala::backend::vulkan::Image::setLayout(VkImageMemoryBarrier barrier) {
    _layout = static_cast<ImageLayout>(barrier.newLayout);
}