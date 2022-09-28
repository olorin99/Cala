#ifndef CALA_IMAGE_H
#define CALA_IMAGE_H

#include <vulkan/vulkan.h>
#include <Ende/platform.h>
#include <Ende/Span.h>
#include <Cala/backend/vulkan/vk_mem_alloc.h>
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    class Driver;
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
        };

        struct DataInfo {
            u32 mipLevel = 0;
            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            u32 format = 4; //byte size of values per pixel TODO: change to enum
            ende::Span<void> data = nullptr;
        };

        Image(Driver& driver, CreateInfo info);

        ~Image();

        Image(Image&& rhs) noexcept;

        Image& operator=(Image&& rhs) noexcept;

        void data(Driver& driver, DataInfo info);

        void* map(u32 format = 4);

        void unmap();

        void copy(CommandBuffer& buffer, Image& dst);


        struct View {
            VkImageView view;

            View();
            ~View();

            View(View&& rhs) noexcept;
            View& operator=(View&& rhs) noexcept;
            View(const View&) = delete;
            View& operator=(const View&) = delete;

            Image* parent() const { return _image; }
        private:

            friend Image;
            VkDevice _device;
            Image* _image;
        };

        View getView(VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, u32 mipLevel = 0, u32 levelCount = 1, u32 arrayLayer = 0, u32 layerCount = 1);

        VkImageMemoryBarrier barrier(Access srcAccess, Access dstAccess, ImageLayout srcLayout, ImageLayout dstLayout);

        VkImage image() const { return _image; }

        Format format() const { return _format; }

        ImageUsage usage() const { return _usage; }

        ImageLayout layout() const { return _layout; }

        u32 width() const { return _width; }

        u32 height() const { return _height; }

        u32 depth() const { return _depth; }

    private:

        Driver& _driver;
        VkImage _image;
        VmaAllocation _allocation;

        u32 _width;
        u32 _height;
        u32 _depth;
        Format _format;
        ImageUsage _usage;
        ImageLayout _layout;

    };

}

#endif //CALA_IMAGE_H
