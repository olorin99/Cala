#ifndef CALA_DEVICE_H
#define CALA_DEVICE_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/CommandPool.h>
#include "Platform.h"
#include <Ende/Vector.h>
#include <Cala/backend/vulkan/Handle.h>
#include <Ende/time/StopWatch.h>

namespace cala::ui {
    class ResourceViewer;
}

namespace cala::backend::vulkan {

    const u32 FRAMES_IN_FLIGHT = 2;

    class Device {
    public:

        Device(Platform& platform);

        ~Device();

        struct FrameInfo {
            u64 frame = 0;
            CommandBuffer* cmd = nullptr;
            VkFence fence = VK_NULL_HANDLE;
        };

        FrameInfo beginFrame();

        ende::time::Duration endFrame();

        u32 frameIndex() const { return _frameCount % FRAMES_IN_FLIGHT; }

        bool waitFrame(u64 frame, u64 timeout = 1000000000);

        bool wait(u64 timeout = 1000000000); // waits for all frames


        BufferHandle stagingBuffer(u32 size);


        CommandBuffer& beginSingleTimeCommands(QueueType queueType = QueueType::GRAPHICS);

        void endSingleTimeCommands(CommandBuffer& buffer);

        template <typename F>
        void immediate(F func, QueueType queueType = QueueType::GRAPHICS) {
            auto& cmd = beginSingleTimeCommands(queueType);
            func(cmd);
            endSingleTimeCommands(cmd);
        }

        CommandBuffer& getCommandBuffer(u32 frame, QueueType queueType = QueueType::GRAPHICS);

        bool gc();

        BufferHandle createBuffer(u32 size, BufferUsage usage, backend::MemoryProperties flags = backend::MemoryProperties::DEVICE, bool persistentlyMapped = false);

        void destroyBuffer(BufferHandle handle);

        BufferHandle resizeBuffer(BufferHandle handle, u32 size, bool transfer = false);

        ImageHandle createImage(Image::CreateInfo info, Sampler* sampler = nullptr);

        void destroyImage(ImageHandle handle);

        ImageHandle getImageHandle(u32 index);

        Image::View& getImageView(ImageHandle handle);

        Image::View& getImageView(u32 index);

        ProgramHandle createProgram(ShaderProgram&& program);


        Sampler& defaultSampler() { return _defaultSampler; }

        Sampler& defaultShadowSampler() { return _defaultShadowSampler; }


        VkDescriptorSetLayout getSetLayout(ende::Span<VkDescriptorSetLayoutBinding> bindings);

        void updateBindlessImage(u32 index, Image::View& image, Sampler& sampler);

        VkDescriptorSetLayout bindlessLayout() const { return _bindlessLayout; }
        VkDescriptorSet bindlessSet() const { return _bindlessSet; }

        void setBindlessSetIndex(u32 index);

        i32 getBindlessIndex() const { return _bindlessIndex; }


        RenderPass* getRenderPass(ende::Span<RenderPass::Attachment> attachments);

        Framebuffer* getFramebuffer(RenderPass* renderPass, ende::Span<VkImageView> attachments, ende::Span<u32> attachmentHashes, u32 width, u32 height);

        void clearFramebuffers();

        VkDescriptorSet getDescriptorSet(CommandBuffer::DescriptorKey key);

        VkPipeline getPipeline(CommandBuffer::PipelineKey key);


        const Context& context() const { return _context; }

        u32 setLayoutCount() const { return _setLayouts.size(); }

        f64 fps() const { return 1000.f / (static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f); }

        f64 milliseconds() const { return static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f; }

        struct Stats {
            u32 buffersInUse = 0;
            u32 allocatedBuffers = 0;
            u32 imagesInUse = 0;
            u32 allocatedImages = 0;
            u32 descriptorSetCount = 0;
            u32 pipelineCount = 0;
            u32 perFrameAllocated = 0;
        };

        Stats stats() const;

    private:
        friend BufferHandle;
        friend ImageHandle;
        friend ProgramHandle;
        friend ui::ResourceViewer;

        Context _context;
        CommandPool _commandPools[2][3]; // 0 = graphics, 1 = compute, 2 = transfer
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

        VkDescriptorSetLayout _bindlessLayout;
        VkDescriptorSet _bindlessSet;
        VkDescriptorPool _bindlessPool;
        i32 _bindlessIndex;


        ende::Vector<Buffer> _buffers;
        ende::Vector<u32> _freeBuffers;
        ende::Vector<std::pair<i32, BufferHandle>> _buffersToDestroy;

        ende::Vector<Image> _images;
        ende::Vector<Image::View> _imageViews;
        ende::Vector<u32> _freeImages;
        ende::Vector<std::pair<i32, ImageHandle>> _imagesToDestroy;

        Sampler _defaultSampler;
        Sampler _defaultShadowSampler;

        ende::Vector<ShaderProgram*> _programs;

        tsl::robin_map<CommandBuffer::DescriptorKey, std::pair<VkDescriptorSet, i32>, ende::util::MurmurHash<CommandBuffer::DescriptorKey>> _descriptorSets;
        VkDescriptorPool _descriptorPool;

        tsl::robin_map<CommandBuffer::PipelineKey, VkPipeline, ende::util::MurmurHash<CommandBuffer::PipelineKey>, CommandBuffer::PipelineEqual> _pipelines;


        u32 _bytesAllocatedPerFrame;

    };

}

#endif //CALA_DEVICE_H
