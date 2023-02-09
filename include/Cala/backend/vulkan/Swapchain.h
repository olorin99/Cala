#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Platform.h>
#include <Ende/Vector.h>

namespace cala::backend::vulkan {

    class Driver;

    class Swapchain {
    public:

        Swapchain(Driver& driver, Platform& platform, bool clear = true);

        ~Swapchain();

        struct Frame {
            u64 id = 0;
            u32 index = 0;
            VkSemaphore imageAquired = VK_NULL_HANDLE;
            Framebuffer* framebuffer;
        };

        Frame nextImage();

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


        Driver& _driver;

        VkSurfaceKHR _surface;
        VkSwapchainKHR _swapchain;
        Format _format;
        VkExtent2D _extent;
        u64 _frame;

        ende::Vector<VkImage> _images;
        ende::Vector<VkImageView> _imageViews;
        ende::Vector<VkSemaphore> _semaphores;

        Image* _depthImage;
        Image::View _depthView;

        RenderPass* _renderPass;
        ende::Vector<Framebuffer*> _framebuffers;

        bool _vsync;

    };

}

#endif //CALA_SWAPCHAIN_H
