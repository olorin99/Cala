#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Ende/Vector.h>

namespace cala::backend::vulkan {

    class Swapchain {
    public:

        Swapchain(Context& context, Platform& platform);

        ~Swapchain();

        struct Frame {
            u64 id = 0;
            u32 index = 0;
            VkSemaphore imageAquired = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            Framebuffer& framebuffer;
        };

        Frame nextImage();

        bool present(Frame frame, VkSemaphore renderFinish);

        bool wait(u64 timeout = 1000000000);

        VkExtent2D extent() const { return _extent; }

        Format format() const { return _format; }

        u32 size() const { return _images.size(); }

        VkImageView view(u32 i) const { return _imageViews[i]; }

        VkFence fence() const { return _fence; }

        RenderPass& renderPass() const { return *_renderPass; }

        Framebuffer& framebuffer(u32 i) { return _framebuffers[i]; }

    private:

        bool createSwapchain();

        bool createImageViews();

        bool createSemaphores();


        Context& _context;

        VkSurfaceKHR _surface;
        VkSwapchainKHR _swapchain;
        Format _format;
        VkExtent2D _extent;
        u64 _frame;

        ende::Vector<VkImage> _images;
        ende::Vector<VkImageView> _imageViews;
        ende::Vector<VkSemaphore> _semaphores;

        Image _depthImage;
        Image::View _depthView;

        RenderPass* _renderPass;
        ende::Vector<Framebuffer> _framebuffers;

        VkFence _fence;

    };

}

#endif //CALA_SWAPCHAIN_H
