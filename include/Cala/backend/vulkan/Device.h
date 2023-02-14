#ifndef CALA_DEVICE_H
#define CALA_DEVICE_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include "Platform.h"

#include <Ende/time/StopWatch.h>

namespace cala::backend::vulkan {

    const u32 FRAMES_IN_FLIGHT = 2;

    class Device {
    public:

        Device(Platform& platform, bool clear = true); // change clear to options struct

        ~Device();

        struct FrameInfo {
            u64 frame = 0;
            CommandBuffer* cmd = nullptr;
            VkFence fence = VK_NULL_HANDLE;
            Swapchain::Frame swapchainInfo;
        };

        FrameInfo beginFrame();

        ende::time::Duration endFrame();

        bool waitFrame(u64 frame, u64 timeout = 1000000000);

        bool wait(u64 timeout = 1000000000); // waits for all frames


        Buffer stagingBuffer(u32 size);


        CommandBuffer beginSingleTimeCommands(QueueType queueType = QueueType::GRAPHICS);

        void endSingleTimeCommands(CommandBuffer& buffer);

        template <typename F>
        void immediate(F func, QueueType queueType = QueueType::GRAPHICS) {
            auto cmd = beginSingleTimeCommands(queueType);
            func(cmd);
            endSingleTimeCommands(cmd);
        }

        VkDeviceMemory allocate(u32 size, u32 typeBits, MemoryProperties flags);

        VkDescriptorSetLayout getSetLayout(ende::Span<VkDescriptorSetLayoutBinding> bindings);

        void updateBindlessImage(u32 index, Image::View& image, Sampler& sampler);

        VkDescriptorSetLayout bindlessLayout() const { return _bindlessLayout; }
        VkDescriptorSet bindlessSet() const { return _bindlessSet; }

        void setBindlessSetIndex(u32 index);

        i32 getBindlessIndex() const { return _bindlessIndex; }

//        VkDescriptorSetLayout emptyLayout() const { return _emptySetLayout; }
//        VkDescriptorSet emptySet() const { return _emptySet; }


        RenderPass* getRenderPass(ende::Span<RenderPass::Attachment> attachments);

        Framebuffer* getFramebuffer(RenderPass* renderPass, ende::Span<VkImageView> attachments, ende::Span<u32> attachmentHashes, u32 width, u32 height);

        void clearFramebuffers();


//        RenderPass* getRenderPass(ende::Span<);


        const Context& context() const { return _context; }

        Swapchain& swapchain() { return *_swapchain; }

        u32 setLayoutCount() const { return _setLayouts.size(); }

        f64 fps() const { return 1000.f / (static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f); }

        f64 milliseconds() const { return static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f; }

    private:

        Context _context;
        Swapchain* _swapchain;
        VkCommandPool _commandPool;
        CommandBuffer _frameCommands[FRAMES_IN_FLIGHT];
        VkFence _frameFences[FRAMES_IN_FLIGHT];
        u64 _frameCount;
        ende::time::StopWatch _frameClock;
        ende::time::Duration _lastFrameTime;

        tsl::robin_map<u64, RenderPass*> _renderPasses;
        tsl::robin_map<u64, Framebuffer*> _framebuffers;

        struct SetLayoutKey {
            VkDescriptorSetLayoutBinding bindings[8];
            bool operator==(const SetLayoutKey& rhs) const {
                return memcmp(this, &rhs, sizeof(SetLayoutKey)) == 0;
            }
        };
        std::unordered_map<SetLayoutKey, VkDescriptorSetLayout, ende::util::MurmurHash<SetLayoutKey>> _setLayouts;
//        VkDescriptorSetLayout _emptySetLayout;
//        VkDescriptorSet _emptySet;

        VkDescriptorSetLayout _bindlessLayout;
        VkDescriptorSet _bindlessSet;
        VkDescriptorPool _bindlessPool;
        i32 _bindlessIndex;

    };

}

#endif //CALA_DEVICE_H