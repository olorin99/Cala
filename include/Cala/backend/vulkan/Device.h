#ifndef CALA_DEVICE_H
#define CALA_DEVICE_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/CommandPool.h>
#include <Cala/backend/vulkan/Semaphore.h>
#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Handle.h>
#include <Ende/time/StopWatch.h>
#include <spdlog/spdlog.h>

namespace cala::ui {
    class ResourceViewer;
}

namespace cala::backend::vulkan {

    const u32 FRAMES_IN_FLIGHT = 2;

    class Device {
    public:

        struct CreateInfo {
            bool useTimeline = true;
        };

        Device(Platform& platform, spdlog::logger& logger, CreateInfo createInfo = { true });

        ~Device();

        struct FrameInfo {
            u64 frame = 0;
            CommandHandle cmd = {};
            VkFence fence = VK_NULL_HANDLE;
        };

        FrameInfo beginFrame();

        ende::time::Duration endFrame();

        u32 frameIndex() const { return _frameCount % FRAMES_IN_FLIGHT; }

        u32 prevFrameIndex() const { return (_frameCount - 1) % FRAMES_IN_FLIGHT; }

        u32 nextFrameIndex() const { return (_frameCount + 1) % FRAMES_IN_FLIGHT; }

        bool waitFrame(u64 frame, u64 timeout = 1000000000);

        bool wait(u64 timeout = 1000000000); // waits for all frames

        bool usingTimeline() const { return _timelineSemaphore.semaphore() != VK_NULL_HANDLE; }

        Semaphore& getTimelineSemaphore() { return _timelineSemaphore; }

        void setFrameValue(u64 frame, u64 value) { _frameValues[frame] = value; }

        u64 getFrameValue(u64 frame) { return _frameValues[frame]; }

        BufferHandle stagingBuffer(u32 size);


        CommandHandle beginSingleTimeCommands(QueueType queueType = QueueType::GRAPHICS);

        void endSingleTimeCommands(CommandHandle buffer);

        template <typename F>
        void immediate(F func, QueueType queueType = QueueType::GRAPHICS) {
            auto cmd = beginSingleTimeCommands(queueType);
            func(cmd);
            endSingleTimeCommands(cmd);
        }

        template<typename F>
        void deferred(F func, QueueType queueType = QueueType::GRAPHICS) {
            _deferredCommands.push_back(func);
        }

        CommandHandle getCommandBuffer(u32 frame, QueueType queueType = QueueType::GRAPHICS);

        bool gc();

        struct ExtraInfo {
            u32 requiredFlags = 0;
            u32 preferredFlags = 0;
        };
        BufferHandle createBuffer(u32 size, BufferUsage usage, backend::MemoryProperties flags = backend::MemoryProperties::DEVICE, bool persistentlyMapped = false, ExtraInfo extraInfo = { 0, 0 });



        BufferHandle resizeBuffer(BufferHandle handle, u32 size, bool transfer = false);

        ImageHandle createImage(Image::CreateInfo info);



        ImageHandle getImageHandle(u32 index);

        Image::View& getImageView(ImageHandle handle);

        Image::View& getImageView(u32 index);


        ShaderModuleHandle createShaderModule(std::span<u32> spirv, ShaderStage stage);

        ShaderModuleHandle recreateShaderModule(ShaderModuleHandle handle, std::span<u32> spirv, ShaderStage stage);

        PipelineLayoutHandle createPipelineLayout(const ShaderInterface& interface);


        SamplerHandle getSampler(Sampler::CreateInfo info);



        SamplerHandle defaultSampler() { return _defaultSampler; }

        SamplerHandle defaultShadowSampler() { return _defaultShadowSampler; }


        VkDescriptorSetLayout getSetLayout(std::span<VkDescriptorSetLayoutBinding> bindings);

        void updateBindlessBuffer(u32 index);

        void updateBindlessImage(u32 index, Image::View& image, bool sampled = true, bool storage = true);

        void updateBindlessSampler(u32 index);

        VkDescriptorSetLayout bindlessLayout() const { return _bindlessLayout; }
        VkDescriptorSet bindlessSet() const { return _bindlessSet; }

        void setBindlessSetIndex(u32 index);

        i32 getBindlessIndex() const { return _bindlessIndex; }


        RenderPass* getRenderPass(std::span<RenderPass::Attachment> attachments);

        Framebuffer* getFramebuffer(RenderPass* renderPass, std::span<VkImageView> attachments, std::span<u32> attachmentHashes, u32 width, u32 height);

        void clearFramebuffers();

        VkDescriptorSet getDescriptorSet(CommandBuffer::DescriptorKey key);

        VkPipeline getPipeline(CommandBuffer::PipelineKey key);


        const Context& context() const { return _context; }

        u32 setLayoutCount() const { return _setLayouts.size(); }

        f64 fps() const { return 1000.f / milliseconds(); }

        f64 milliseconds() const { return static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f; }

        u32 framesInFlight() const { return FRAMES_IN_FLIGHT; }

        struct Stats {
            u32 buffersInUse = 0;
            u32 allocatedBuffers = 0;
            u32 imagesInUse = 0;
            u32 allocatedImages = 0;
            u32 shaderModulesInUse = 0;
            u32 allocatedShaderModules = 0;
            u32 pipelineLayoutsInUse = 0;
            u32 allocatedPipelineLayouts = 0;
            u32 descriptorSetCount = 0;
            u32 pipelineCount = 0;
            u32 perFrameAllocated = 0;
            u32 perFrameUploaded = 0;
            u32 totalAllocated = 0;
            u32 totalDeallocated = 0;
        };

        Stats stats() const;

        spdlog::logger& logger() { return _logger; }

        void printMarkers();

        void increaseDataUploadCount(u32 size) { _bytesUploadedToGPUPerFrame += size; }

    private:
        friend BufferHandle;
        friend ImageHandle;
//        friend ProgramHandle;
        friend SamplerHandle;
        friend ShaderModuleHandle;
        friend PipelineLayoutHandle;
        friend ui::ResourceViewer;
        friend CommandBuffer;

        spdlog::logger& _logger;
        Context _context;
        CommandPool _commandPools[FRAMES_IN_FLIGHT][3]; // 0 = graphics, 1 = compute, 2 = transfer
        Semaphore _timelineSemaphore;
        u64 _timelineValue;
        u64 _frameValues[FRAMES_IN_FLIGHT];
        VkFence _frameFences[FRAMES_IN_FLIGHT];
        u64 _frameCount;
        ende::time::StopWatch _frameClock;
        ende::time::Duration _lastFrameTime;

        std::vector<std::function<void(CommandHandle)>> _deferredCommands;

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

        template <typename T, typename H>
        struct ResourceList {
            std::vector<std::pair<std::unique_ptr<T>, std::unique_ptr<typename H::Data>>> _resources;
            std::vector<u32> _freeResources;
            std::vector<std::pair<i32, i32>> _destroyQueue;

            u32 used() const { return allocated() - free(); }

            u32 allocated() const { return _resources.size(); }

            u32 free() const { return _freeResources.size(); }

            u32 toDestroy() const { return _destroyQueue.size(); }

            H getHandle(Device* device, i32 index) { return { device, -1, _resources[index].second.get() }; }

            T* getResource(i32 index) { return _resources[index].first.get(); }

            T* getResource(i32 index) const { return _resources[index].first.get(); }

            i32 insert(Device* device) {
                i32 index = -1;
                if (!_freeResources.empty()) {
                    index = _freeResources.back();
                    _freeResources.pop_back();
                    _resources[index].first = std::make_unique<T>(device);
                    _resources[index].second = std::make_unique<typename H::Data>(index, 1, [this](i32 index) {
                        _destroyQueue.push_back(std::make_pair(FRAMES_IN_FLIGHT + 1, index));
                    });
                } else {
                    index = _resources.size();
                    _resources.emplace_back(std::make_unique<T>(device), std::make_unique<typename H::Data>(index, 1, [this](i32 index) {
                        _destroyQueue.push_back(std::make_pair(FRAMES_IN_FLIGHT + 1, index));
                    }));
                }
                return index;
            }

            template <typename F>
            bool clearDestroyQueue(F func) {
                for (auto it = _destroyQueue.begin(); it != _destroyQueue.end(); it++) {
                    if (it->first <= 0) {
                        auto& object = *_resources[it->second].first;
                        func(it->second, object);
                        _freeResources.push_back(it->second);
                        _destroyQueue.erase(it--);
                    } else
                        it->first--;
                }
                return true;
            }

            template <typename F>
            void clearAll(F func) {
                for (u32 i = 0; i < _resources.size(); i++) {
                    func(i, *_resources[i].first);
                }
                _resources.clear();
                _freeResources.clear();
                _destroyQueue.clear();
            }
        };

        ResourceList<Buffer, BufferHandle> _bufferList;

        ResourceList<Image, ImageHandle> _imageList;
        std::vector<Image::View> _imageViews;

        SamplerHandle _defaultSampler;
        SamplerHandle _defaultShadowSampler;

        ResourceList<ShaderModule, ShaderModuleHandle> _shaderModulesList;

        ResourceList<PipelineLayout, PipelineLayoutHandle> _pipelineLayoutList;


        std::vector<std::pair<Sampler::CreateInfo, std::unique_ptr<Sampler>>> _samplers;

        tsl::robin_map<CommandBuffer::DescriptorKey, std::pair<VkDescriptorSet, i32>, ende::util::MurmurHash<CommandBuffer::DescriptorKey>> _descriptorSets;
        VkDescriptorPool _descriptorPool;

        tsl::robin_map<CommandBuffer::PipelineKey, VkPipeline, ende::util::MurmurHash<CommandBuffer::PipelineKey>, CommandBuffer::PipelineEqual> _pipelines;

        BufferHandle _markerBuffer[FRAMES_IN_FLIGHT];
        u32 _offset = 0;
        u32 _marker = 1;
        std::vector<std::pair<std::string_view, u32>> _markedCmds[FRAMES_IN_FLIGHT];


        u32 _bytesAllocatedPerFrame;
        u32 _bytesUploadedToGPUPerFrame;

        u32 _totalAllocated;
        u32 _totalDeallocated;

    };

}

#endif //CALA_DEVICE_H
