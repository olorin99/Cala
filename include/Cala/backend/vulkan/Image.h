#ifndef CALA_IMAGE_H
#define CALA_IMAGE_H

#include <vulkan/vulkan.h>
#include <Ende/platform.h>
#include <Ende/Span.h>
#include <Cala/backend/vulkan/Context.h>

namespace cala::backend::vulkan {

    class Image {
    public:

        struct CreateInfo {
            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            VkFormat format = VK_FORMAT_R8G8B8A8_UINT;
            u32 mipLevels = 1;
            u32 arrayLayers = 1;
            VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        };

        struct DataInfo {
            u32 mipLevel = 0;
            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            VkFormat format;
            ende::Span<void> data = nullptr;
        };

        Image(Context& context, CreateInfo info);

        ~Image();

        void data(DataInfo info);


        struct View {
            VkImageView view;
            ~View();
        private:
            friend Image;
            VkDevice _device;
        };

        View getView(VkImageViewType viewType, VkImageAspectFlags aspect, u32 mipLevel = 0, u32 levelCount = 1, u32 arrayLayer = 0, u32 layerCount = 1);


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