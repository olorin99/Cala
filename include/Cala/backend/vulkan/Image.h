#ifndef CALA_IMAGE_H
#define CALA_IMAGE_H

#include <volk.h>
#include <Ende/platform.h>
#include <Ende/Span.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "../../third_party/vk_mem_alloc.h"
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    class Device;
    class CommandBuffer;

    class Image {
    public:

        struct CreateInfo {
            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            Format format = Format::RGBA8_UINT;
            u32 mipLevels = 1;
            u32 arrayLayers = 1;
            ImageUsage usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
            ImageType type = ImageType::AUTO;
            Format aliasFormat = Format::UNDEFINED;
        };

        struct DataInfo {
            u32 mipLevel = 0;
            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            u32 format = 4; //byte size of values per pixel TODO: change to enum
            ende::Span<void> data = nullptr;
            u32 layer = 0; // move up in struct just here for backward compat
        };

        Image(Device* device);

        Image() = delete;

        Image(Image&& rhs) noexcept;

        Image& operator=(Image&& rhs) noexcept;

        void data(Device& driver, DataInfo info);

        void* map(u32 format = 4);

        void unmap();

        void copy(CommandBuffer& buffer, Image& dst, u32 srcLayer = 0, u32 dstLayer = 0, u32 srcMipLevel = 0, u32 dstMipLevel = 0);

        void blit(CommandBuffer& buffer, Image& dst, ImageLayout srcLayout, ImageLayout dstLayout, VkFilter filter = VK_FILTER_NEAREST);

        void generateMips();

        void generateMips(CommandBuffer& cmd);


        struct View {
            VkImageView view;

            View();
            ~View();

            View(View&& rhs) noexcept;
            View& operator=(View&& rhs) noexcept;
//            View(const View&) = delete;
//            View& operator=(const View&) = delete;

            Image* parent() const { return _image; }
        private:

            friend Image;
            VkDevice _device;
            Image* _image;
        };

        View newView(u32 mipLevel = 0, u32 levelCount = 1, u32 arrayLayer = 0, u32 layerCount = 0, ImageViewType viewType = ImageViewType::AUTO, Format format = Format::UNDEFINED);

//        VkImageMemoryBarrier barrier(Access srcAccess, Access dstAccess, ImageLayout srcLayout, ImageLayout dstLayout, u32 layer = 0);

        struct Barrier {
            Image* image;
            PipelineStage srcStage;
            PipelineStage dstStage;
            Access srcAccess;
            Access dstAccess;
            ImageLayout srcLayout;
            ImageLayout dstLayout;
            VkImageSubresourceRange subresourceRange;
        };

        Barrier barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess, ImageLayout dstLayout, u32 layer = 0);

        Barrier barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess, ImageLayout srcLayout, ImageLayout dstLayout, u32 layer = 0);

        void setLayout(VkImageLayout layout);

        VkImage image() const { return _image; }

        Format format() const { return _format; }

        ImageUsage usage() const { return _usage; }

        ImageLayout layout() const { return _layout; }

        u32 width() const { return _width; }

        u32 height() const { return _height; }

        u32 depth() const { return _depth; }

        u32 mips() const { return _mips; }

        u32 layers() const { return _layers; }

    private:

        friend Device;

        Device* _device;
        VkImage _image;
        VmaAllocation _allocation;

        u32 _width;
        u32 _height;
        u32 _depth;
        u32 _layers;
        u32 _mips;
        Format _format;
        ImageUsage _usage;
        ImageLayout _layout;
        ImageType _type;

    };

}

#endif //CALA_IMAGE_H
