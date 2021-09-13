#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <Cala/backend/vulkan/Context.h>

#include <array>

namespace cala::backend::vulkan {

    class Swapchain {
    public:

        Swapchain(Context& context, void* window, void* display);

        ~Swapchain();

        struct Frame {
            u64 id = 0;
            u32 index = 0;
            VkSemaphore imageAquired = VK_NULL_HANDLE;
            VkSemaphore renderFinished = VK_NULL_HANDLE;
        };

        Frame nextImage();

        bool present(Frame frame);

        VkExtent2D extent() const { return _extent; }

        VkFormat format() const { return _format.format; }

        u32 size() const { return _images.size(); }

        VkImageView view(u32 i) const { return _imageViews[i]; }

    private:

        bool createSwapchain();

        bool createImageViews();

        bool createSemaphores();


        Context& _context;

        VkSurfaceKHR _surface;
        VkSwapchainKHR _swapchain;
        VkSurfaceFormatKHR _format;
        VkExtent2D _extent;
        u64 _frame;

        ende::Vector<VkImage> _images;
        ende::Vector<VkImageView> _imageViews;
        ende::Vector<std::pair<VkSemaphore, VkSemaphore>> _semaphores;
//        std::array<VkImage, 2> _images;
//        std::array<VkImageView, 2> _imageViews;
//        std::array<std::pair<VkSemaphore, VkSemaphore>, 2> _semaphores;

    };

}

#endif //CALA_SWAPCHAIN_H
