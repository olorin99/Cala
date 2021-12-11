#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <Cala/backend/vulkan/Context.h>

#include <Ende/Vector.h>

namespace cala::backend::vulkan {

    class Swapchain {
    public:

        Swapchain(Context& context, void* window, void* display);

        ~Swapchain();

        struct Frame {
            u64 id = 0;
            u32 index = 0;
            VkSemaphore imageAquired = VK_NULL_HANDLE;
        };

        Frame nextImage();

        bool present(Frame frame, VkSemaphore renderFinish);

        bool wait(u64 timeout = 1000000000);

        VkExtent2D extent() const { return _extent; }

        VkFormat format() const { return _format.format; }

        u32 size() const { return _images.size(); }

        VkImageView view(u32 i) const { return _imageViews[i]; }

        VkFence fence() const { return _fence; }

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
        ende::Vector<VkSemaphore> _semaphores;

        VkFence _fence;
//        std::array<VkImage, 2> _images;
//        std::array<VkImageView, 2> _imageViews;
//        std::array<std::pair<VkSemaphore, VkSemaphore>, 2> _semaphores;

    };

}

#endif //CALA_SWAPCHAIN_H
