#ifndef CALA_IMAGE_H
#define CALA_IMAGE_H

#include <vulkan/vulkan.h>
#include <Ende/platform.h>
#include <Ende/Span.h>
#include <Cala/backend/vulkan/Context.h>

#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    class Driver;

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

        Image(Context& context, CreateInfo info);

        ~Image();

        void data(Driver& driver, DataInfo info);


        struct View {
            VkImageView view;

            View() = default;
            ~View();

            View(View&& rhs) noexcept;
            View& operator=(View&& rhs) noexcept;
            View(const View&) = delete;
            View& operator=(const View&) = delete;
        private:

            friend Image;
            VkDevice _device;
        };

        View getView(VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, u32 mipLevel = 0, u32 levelCount = 1, u32 arrayLayer = 0, u32 layerCount = 1);

        VkImageMemoryBarrier barrier(VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout srcLayout, VkImageLayout dstLayout);

        VkImage image() const { return _image; }

    private:

        Context& _context;
        VkImage _image;
        VkDeviceMemory _memory;

        u32 _width;
        u32 _height;
        u32 _depth;
        VkFormat _format;


    };

}

#endif //CALA_IMAGE_H
