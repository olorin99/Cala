#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <expected>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Semaphore.h>
#include "Handle.h"

namespace cala::backend::vulkan {

    class Device;

    class Swapchain {
    public:

        Swapchain(Device& driver, Platform& platform, bool clear = true);

        ~Swapchain();

        struct SemaphorePair {
            Semaphore acquire;
            Semaphore present;
        };
        struct Frame {
            u64 id = 0;
            u32 index = 0;
            SemaphorePair semaphores = {{nullptr}, {nullptr}};
        };

        std::expected<Frame, Error> nextImage();

        std::expected<bool, Error> present(Frame frame);

        bool resize(u32 width, u32 height);

        void blitImageToFrame(u32 index, CommandHandle buffer, Image& src);

        void copyImageToFrame(u32 index, CommandHandle buffer, Image& src);

        void copyFrameToImage(u32 index, CommandHandle buffer, Image& dst);

        void copyFrameToImage(u32 index, Image& dst);

        VkExtent2D extent() const { return _extent; }

        Format format() const { return _format; }

        u32 size() const { return _images.size(); }

        VkImageView view(u32 i) const { return _imageViews[i]; }

        void setPresentMode(PresentMode mode);

        PresentMode getPresentMode() const { return _mode; }

    private:

        bool createSwapchain();

        bool createImageViews();

        bool createSemaphores();


        Device& _driver;

        VkSurfaceKHR _surface;
        VkSwapchainKHR _swapchain;
        Format _format;
        PresentMode _mode;
        VkExtent2D _extent;
        u64 _frame;

        std::vector<VkImage> _images;
        std::vector<VkImageView> _imageViews;
        std::vector<SemaphorePair> _semaphores;
        std::vector<VmaAllocation> _allocations;

    };

}

#endif //CALA_SWAPCHAIN_H
