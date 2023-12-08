#ifndef CALA_SWAPCHAIN_H
#define CALA_SWAPCHAIN_H

#include <expected>
#include <Cala/vulkan/Image.h>
#include <Cala/vulkan/RenderPass.h>
#include <Cala/vulkan/Framebuffer.h>
#include <Cala/vulkan/Platform.h>
#include <Cala/vulkan/Semaphore.h>
#include <Cala/vulkan/Handle.h>

namespace cala::vk {

    class Device;

    class Swapchain {
    public:

        struct Info {
            Platform* platform = nullptr;
            std::string name = {};
        };

        static std::expected<Swapchain, Error> create(Device* device, Info info);

        Swapchain() = default;

        ~Swapchain();

        Swapchain(Swapchain&& rhs) noexcept;

        Swapchain& operator==(Swapchain&& rhs) noexcept;

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

        std::expected<void, Error> createSwapchain();

        std::expected<void, Error> createImageViews();

        bool createSemaphores();


        Device* _device = nullptr;

        VkSurfaceKHR _surface = VK_NULL_HANDLE;
        VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
        Format _format = Format::UNDEFINED;
        PresentMode _mode = PresentMode::FIFO;
        VkExtent2D _extent = {};
        u64 _frame = 0;

        std::vector<VkImage> _images = {};
        std::vector<VkImageView> _imageViews = {};
        std::vector<SemaphorePair> _semaphores = {};
        std::vector<VmaAllocation> _allocations = {};

    };

}

#endif //CALA_SWAPCHAIN_H
