#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <expected>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Platform.h>
#include "Handle.h"

namespace cala::backend::vulkan {

    class Device;

    class Swapchain {
    public:

        Swapchain(Device& driver, Platform& platform, bool clear = true);

        ~Swapchain();

        struct Frame {
            u64 id = 0;
            u32 index = 0;
            VkSemaphore imageAquired = VK_NULL_HANDLE;
            Framebuffer* framebuffer = nullptr;
        };

        std::expected<Frame, i32> nextImage();

        bool present(Frame frame, VkSemaphore renderFinish);

        bool resize(u32 width, u32 height);

        void blitImageToFrame(u32 index, CommandBuffer& buffer, Image& src);

        void copyImageToFrame(u32 index, CommandBuffer& buffer, Image& src);

        void copyFrameToImage(u32 index, CommandBuffer& buffer, Image& dst);

        void copyFrameToImage(u32 index, Image& dst);

        VkExtent2D extent() const { return _extent; }

        Format format() const { return _format; }

        u32 size() const { return _images.size(); }

        VkImageView view(u32 i) const { return _imageViews[i]; }

        RenderPass& renderPass() const { return *_renderPass; }

        Framebuffer& framebuffer(u32 i) { return *_framebuffers[i]; }

        void setVsync(bool vsync);

        bool getVsync() const { return _vsync; }

    private:

        bool createSwapchain();

        bool createImageViews();

        bool createSemaphores();


        Device& _driver;

        VkSurfaceKHR _surface;
        VkSwapchainKHR _swapchain;
        Format _format;
        VkExtent2D _extent;
        u64 _frame;

        std::vector<VkImage> _images;
        std::vector<VkImageView> _imageViews;
        std::vector<VkSemaphore> _semaphores;
        std::vector<VmaAllocation> _allocations;

        ImageHandle _depthImage;
        Image::View _depthView;

        RenderPass* _renderPass;
        std::vector<Framebuffer*> _framebuffers;

        bool _vsync;

    };

}

#endif //CALA_SWAPCHAIN_H
