#include "Cala/backend/vulkan/Image.h"
#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/primitives.h>

cala::backend::vulkan::Image::Image(cala::backend::vulkan::Device *device)
    : _device(device),
    _image(VK_NULL_HANDLE),
    _allocation(nullptr),
    _layout(ImageLayout::UNDEFINED)
{}

cala::backend::vulkan::Image::Image(Image &&rhs) noexcept
    : _device(rhs._device),
      _image(VK_NULL_HANDLE),
      _allocation(nullptr),
      _width(0),
      _height(0),
      _depth(0),
      _format(Format::UNDEFINED),
      _usage(ImageUsage::COLOUR_ATTACHMENT),
      _layout(ImageLayout::UNDEFINED)
{
    std::swap(_image, rhs._image);
    std::swap(_allocation, rhs._allocation);
    std::swap(_width, rhs._width);
    std::swap(_height, rhs._height);
    std::swap(_depth, rhs._depth);
    std::swap(_format, rhs._format);
    std::swap(_usage, rhs._usage);
    std::swap(_layout, rhs._layout);
}

cala::backend::vulkan::Image &cala::backend::vulkan::Image::operator=(Image &&rhs) noexcept {
    assert(_device == rhs._device);
    std::swap(_image, rhs._image);
    std::swap(_allocation, rhs._allocation);
    std::swap(_width, rhs._width);
    std::swap(_height, rhs._height);
    std::swap(_depth, rhs._depth);
    std::swap(_format, rhs._format);
    std::swap(_usage, rhs._usage);
    std::swap(_layout, rhs._layout);
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

void cala::backend::vulkan::Image::_data(cala::backend::vulkan::Device& driver, DataInfo info, std::span<u8> data) {

    auto staging = driver.stagingBuffer(info.width * info.height * info.depth * info.format);
    staging->data(data);
    driver.immediate([&](CommandHandle buffer) {
        VkImageSubresourceRange range;
        range.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = info.layer;
        range.layerCount = 1;

        Barrier imageBarrier = barrier(PipelineStage::TOP, PipelineStage::TRANSFER, Access::NONE, Access::TRANSFER_WRITE, ImageLayout::TRANSFER_DST);
        buffer->pipelineBarrier({ &imageBarrier, 1 });

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

        vkCmdCopyBufferToImage(buffer->buffer(), staging->buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        imageBarrier = barrier(PipelineStage::TRANSFER, PipelineStage::FRAGMENT_SHADER, Access::TRANSFER_WRITE, Access::SHADER_READ, ImageLayout::SHADER_READ_ONLY);
        buffer->pipelineBarrier({ &imageBarrier, 1 });
    });
//    driver.destroyBuffer(staging);
    _layout = ImageLayout::SHADER_READ_ONLY;
}

void *cala::backend::vulkan::Image::map(u32 format) {
    void* address = nullptr;
    vmaMapMemory(_device->context().allocator(), _allocation, &address);
    return address;
}

void cala::backend::vulkan::Image::unmap() {
    vmaUnmapMemory(_device->context().allocator(), _allocation);
}

void cala::backend::vulkan::Image::blit(CommandHandle buffer, Image &dst, ImageLayout srcLayout, ImageLayout dstLayout, VkFilter filter) {
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

    vkCmdBlitImage(buffer->buffer(), _image, getImageLayout(srcLayout), dst._image, getImageLayout(dstLayout), 1, &blit, filter);
}

void cala::backend::vulkan::Image::copy(cala::backend::vulkan::CommandHandle buffer, cala::backend::vulkan::Image &dst, u32 srcLayer, u32 dstLayer, u32 srcMipLevel, u32 dstMipLevel) {
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
    vkCmdCopyImage(buffer->buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void cala::backend::vulkan::Image::copy(cala::backend::vulkan::CommandHandle buffer, cala::backend::vulkan::Buffer &dst, u32 dstOffset, u32 srcLayer, u32 srcMipLevel) {
    VkBufferImageCopy region{};

    region.imageSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = srcMipLevel;
    region.imageSubresource.baseArrayLayer = srcLayer;
    region.imageSubresource.layerCount = 1;

    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { _width, _height, _depth };

    region.bufferOffset = dstOffset;

    vkCmdCopyImageToBuffer(buffer->buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.buffer(), 1, &region);
}

void cala::backend::vulkan::Image::generateMips() {
    _device->immediate([&](CommandHandle cmd) {
//        Barrier b = barrier(Access::SHADER_READ, Access::TRANSFER_WRITE, ImageLayout::TRANSFER_DST);
//        cmd.pipelineBarrier(PipelineStage::FRAGMENT_SHADER, PipelineStage::TRANSFER, { &b, 1});
        generateMips(cmd);
    });
}

void cala::backend::vulkan::Image::generateMips(CommandHandle cmd) {
    // mip -> mip+1

    ImageLayout initialLayout = layout();

    Barrier b = barrier(PipelineStage::TOP, PipelineStage::TRANSFER, Access::NONE, Access::TRANSFER_READ, ImageLayout::TRANSFER_SRC);
    cmd->pipelineBarrier({ &b, 1 });

    b.subresourceRange.layerCount = 1;
    b.subresourceRange.levelCount = 1;

    for (u32 layer = 0; layer < _layers; layer++) {
        i32 mipWidth = _width;
        i32 mipHeight = _height;
        i32 mipDepth = _depth;
        b.subresourceRange.baseArrayLayer = layer;
        for (u32 mip = 1; mip < _mips; mip++) {

            b.subresourceRange.baseMipLevel = mip;
            b.srcStage = PipelineStage::TRANSFER;
            b.dstStage = PipelineStage::TRANSFER;
            b.srcLayout = ImageLayout::TRANSFER_SRC;
            b.dstLayout = ImageLayout::TRANSFER_DST;
            b.srcAccess = Access::TRANSFER_READ;
            b.dstAccess = Access::TRANSFER_WRITE;

            cmd->pipelineBarrier({ &b, 1 });

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, mipDepth};
            blit.srcSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = mip - 1;
            blit.srcSubresource.baseArrayLayer = layer;
            blit.srcSubresource.layerCount = 1;

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
            if (mipDepth > 1) mipDepth /= 2;

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth, mipHeight, mipDepth};
            blit.dstSubresource.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = mip;
            blit.dstSubresource.baseArrayLayer = layer;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd->buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            b.subresourceRange.baseMipLevel = mip;
            b.srcStage = PipelineStage::TRANSFER;
            b.dstStage = PipelineStage::TRANSFER;
            b.srcLayout = ImageLayout::TRANSFER_DST;
            b.dstLayout = ImageLayout::TRANSFER_SRC;
            b.srcAccess = Access::TRANSFER_WRITE;
            b.dstAccess = Access::TRANSFER_READ;

            cmd->pipelineBarrier({ &b, 1 });

        }
    }

    b = barrier(PipelineStage::TRANSFER, PipelineStage::BOTTOM, Access::TRANSFER_READ | Access::TRANSFER_WRITE, Access::NONE, initialLayout);
    cmd->pipelineBarrier({ &b, 1 });
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
//    viewCreateInfo.subresourceRange.aspectMask = _format == _device.context().depthFormat() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = mipLevel;
    viewCreateInfo.subresourceRange.levelCount = levelCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = arrayLayer;
    viewCreateInfo.subresourceRange.layerCount = layerCount == 0 ? VK_REMAINING_ARRAY_LAYERS : layerCount;

    VkImageView view;
    vkCreateImageView(_device->context().device(), &viewCreateInfo, nullptr, &view);
    View v{};
    v.view = view;
    v._device = _device->context().device();
    v._image = this;
    return v;
}

//VkImageMemoryBarrier cala::backend::vulkan::Image::barrier(Access srcAccess, Access dstAccess, ImageLayout srcLayout, ImageLayout dstLayout, u32 layer) {
//    VkImageSubresourceRange range{};
//    range.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
//    range.baseMipLevel = 0;
//    range.levelCount = VK_REMAINING_MIP_LEVELS;
//    range.baseArrayLayer = layer;
//    range.layerCount = VK_REMAINING_ARRAY_LAYERS;
//
//    VkImageMemoryBarrier memoryBarrier{};
//    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//    memoryBarrier.srcAccessMask = getAccessFlags(srcAccess);
//    memoryBarrier.dstAccessMask = getAccessFlags(dstAccess);
//    memoryBarrier.oldLayout = getImageLayout(srcLayout);
//    memoryBarrier.newLayout = getImageLayout(dstLayout);
//    memoryBarrier.image = _image;
//    memoryBarrier.subresourceRange = range;
//    return memoryBarrier;
//}

cala::backend::vulkan::Image::Barrier cala::backend::vulkan::Image::barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess, ImageLayout dstLayout, u32 layer) {
    Barrier b{};
    b.subresourceRange.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    b.subresourceRange.baseArrayLayer = layer;
    b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    b.srcStage = srcStage;
    b.dstStage = dstStage;
    b.srcAccess = srcAccess;
    b.dstAccess = dstAccess;
    b.srcLayout = layout();
    b.dstLayout = dstLayout;
    b.image = this;
    return b;
}

cala::backend::vulkan::Image::Barrier cala::backend::vulkan::Image::barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess, ImageLayout srcLayout, ImageLayout dstLayout, u32 layer) {
    Barrier b{};
    b.subresourceRange.aspectMask = isDepthFormat(_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    b.subresourceRange.baseArrayLayer = layer;
    b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    b.srcStage = srcStage;
    b.dstStage = dstStage;
    b.srcAccess = srcAccess;
    b.dstAccess = dstAccess;
    b.srcLayout = srcLayout;
    b.dstLayout = dstLayout;
    b.image = this;
    return b;
}

void cala::backend::vulkan::Image::setLayout(VkImageLayout layout) {
    _layout = static_cast<ImageLayout>(layout);
}