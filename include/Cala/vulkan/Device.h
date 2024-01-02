#ifndef CALA_DEVICE_H
#define CALA_DEVICE_H

#include <Cala/vulkan/Context.h>
#include <Cala/vulkan/Swapchain.h>
#include <Cala/vulkan/CommandBuffer.h>
#include <Cala/vulkan/CommandPool.h>
#include <Cala/vulkan/Semaphore.h>
#include <Cala/vulkan/Platform.h>
#include <Cala/vulkan/Handle.h>
#include <Ende/time/StopWatch.h>
#include <spdlog/spdlog.h>
#include <Cala/vulkan/Timer.h>

namespace cala::ui {
    class ResourceViewer;
}

namespace cala::vk {

    const u32 FRAMES_IN_FLIGHT = 2;

    class Device {
    public:

        struct CreateInfo {
            bool useTimeline = true;
            Platform* platform = nullptr;
            spdlog::logger* logger = nullptr;
        };

//        Device(Platform& platform, spdlog::logger& logger, CreateInfo createInfo = { true });

        static std::expected<std::unique_ptr<Device>, Error> create(CreateInfo info);

        Device() = default;

        ~Device();

        Device(Device&& rhs) noexcept;

        Device& operator==(Device&& rhs) noexcept;

        struct FrameInfo {
            u64 frame = 0;
            CommandHandle cmd = {};
            VkFence fence = VK_NULL_HANDLE;
        };

        std::expected<FrameInfo, Error> beginFrame();

        std::chrono::high_resolution_clock::duration endFrame();

        u32 frameIndex() const { return _frameCount % FRAMES_IN_FLIGHT; }

        u32 prevFrameIndex() const { return (_frameCount - 1) % FRAMES_IN_FLIGHT; }

        u32 nextFrameIndex() const { return (_frameCount + 1) % FRAMES_IN_FLIGHT; }

        std::expected<void, Error> waitFrame(u64 frame, u64 timeout = 1000000000);

        bool wait(u64 timeout = 1000000000); // waits for all frames

        bool usingTimeline() const { return _timelineSemaphore.semaphore() != VK_NULL_HANDLE; }

        Semaphore& getTimelineSemaphore() { return _timelineSemaphore; }

        void setFrameValue(u64 frame, u64 value) { _frameValues[frame] = value; }

        u64 getFrameValue(u64 frame) { return _frameValues[frame]; }


        CommandHandle beginSingleTimeCommands(QueueType queueType = QueueType::GRAPHICS);

        std::expected<void, Error> endSingleTimeCommands(CommandHandle buffer);

        template <typename F>
        u64 immediate(F func, QueueType queueType = QueueType::GRAPHICS, bool time = false) {
            auto cmd = beginSingleTimeCommands(queueType);
            if (time)
                _immediateTimer.start(cmd);
            func(cmd);
            if (time)
                _immediateTimer.stop();
            endSingleTimeCommands(cmd).transform_error([&] (auto error) {
                switch (error) {
                    case Error::INVALID_COMMAND_BUFFER:
                        _logger->error("Error submitting immediate command buffer");
                        break;
                    default:
                        _logger->error("Device lost on immediate command submit");
                        break;
                }
                return error;
            });
            if (time)
                return _immediateTimer.result();
            return 0;
        }

        template<typename F>
        void deferred(F func, QueueType queueType = QueueType::GRAPHICS) {
            _deferredCommands.push_back(func);
        }

        CommandHandle getCommandBuffer(u32 frame, QueueType queueType = QueueType::GRAPHICS);

        bool gc();

        struct BufferInfo {
            u32 size = 0;
            BufferUsage usage = BufferUsage::TRANSFER_SRC;
            MemoryProperties memoryType = MemoryProperties::DEVICE;
            bool persistentlyMapped = false;
            u32 requiredFlags = 0;
            u32 preferredFlags = 0;
            std::string name = {};
        };
        BufferHandle createBuffer(BufferInfo info);
        BufferHandle resizeBuffer(BufferHandle handle, u32 size, bool transfer = false);


        ImageHandle createImage(Image::CreateInfo info);

        ImageHandle getImageHandle(u32 index);


        ShaderModuleHandle createShaderModule(std::span<u32> spirv, ShaderStage stage);
        ShaderModuleHandle recreateShaderModule(ShaderModuleHandle handle, std::span<u32> spirv, ShaderStage stage);

        PipelineLayoutHandle createPipelineLayout(const ShaderInterface& interface);
        PipelineLayoutHandle recreatePipelineLayout(PipelineLayoutHandle handle, const ShaderInterface& interface);


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

        Framebuffer* getFramebuffer(RenderPass* renderPass, std::span<VkImageView> attachments, std::span<u32> hashes, u32 width, u32 height);

        void clearFramebuffers();

        VkDescriptorSet getDescriptorSet(CommandBuffer::DescriptorKey key);

        std::expected<VkPipeline, Error> getPipeline(CommandBuffer::PipelineKey key);


        const Context& context() const { return _context; }

        u32 setLayoutCount() const { return _setLayouts.size(); }

        f64 fps() const { return 1000.f / milliseconds(); }

        f64 milliseconds() const { return std::chrono::duration_cast<std::chrono::milliseconds>(_lastFrameTime).count(); }

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

        spdlog::logger& logger() { return *_logger; }

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

        spdlog::logger* _logger = nullptr;
        Context _context = {};
        std::array<std::array<CommandPool, 3>, FRAMES_IN_FLIGHT> _commandPools = {}; // 0 = graphics, 1 = compute, 2 = transfer
        Semaphore _timelineSemaphore = {};
        Semaphore _immediateSemaphore = {};
        u64 _frameValues[FRAMES_IN_FLIGHT] = {};
        VkFence _frameFences[FRAMES_IN_FLIGHT] = {};
        u64 _frameCount = 0;
        ende::time::StopWatch _frameClock = {};
        std::chrono::system_clock::duration _lastFrameTime = {};

        Timer _immediateTimer = {};

        std::vector<std::function<void(CommandHandle)>> _deferredCommands = {};

        tsl::robin_map<u64, RenderPass*> _renderPasses = {};
        tsl::robin_map<u64, std::pair<i32, Framebuffer*>> _framebuffers = {};

        struct SetLayoutKey {
            VkDescriptorSetLayoutBinding bindings[8];
            bool operator==(const SetLayoutKey& rhs) const {
                return memcmp(this, &rhs, sizeof(SetLayoutKey)) == 0;
            }
        };
        std::unordered_map<SetLayoutKey, VkDescriptorSetLayout, ende::util::MurmurHash<SetLayoutKey>> _setLayouts = {};

        VkDescriptorSetLayout _bindlessLayout = VK_NULL_HANDLE;
        VkDescriptorSet _bindlessSet = VK_NULL_HANDLE;
        VkDescriptorPool _bindlessPool = VK_NULL_HANDLE;
        i32 _bindlessIndex = -1;

        template <typename T, typename H>
        struct ResourceList {
            std::vector<std::pair<std::unique_ptr<T>, std::unique_ptr<typename H::Data>>> _resources = {};
            std::vector<u32> _freeResources = {};
            std::vector<std::pair<i32, i32>> _destroyQueue = {};

            u32 used() const { return allocated() - free(); }

            u32 allocated() const { return _resources.size(); }

            u32 free() const { return _freeResources.size(); }

            u32 toDestroy() const { return _destroyQueue.size(); }

            H getHandle(Device* device, i32 index) {
                _resources[index].second->count++;
                return { device, -1, _resources[index].second.get() };
            }

            T* getResource(i32 index) { return _resources[index].first.get(); }

            T* getResource(i32 index) const { return _resources[index].first.get(); }

            i32 insert(Device* device) {
                i32 index = -1;
                if (!_freeResources.empty()) {
                    index = _freeResources.back();
                    _freeResources.pop_back();
                    _resources[index].first = std::make_unique<T>(device);
                    _resources[index].second = std::make_unique<typename H::Data>(index, 0, [this](i32 index) {
                        _destroyQueue.push_back(std::make_pair(FRAMES_IN_FLIGHT + 1, index));
                    });
                } else {
                    index = _resources.size();
                    _resources.emplace_back(std::make_unique<T>(device), std::make_unique<typename H::Data>(index, 0, [this](i32 index) {
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

        ResourceList<Buffer, BufferHandle> _bufferList = {};
        ResourceList<Image, ImageHandle> _imageList = {};
        ResourceList<ShaderModule, ShaderModuleHandle> _shaderModulesList = {};
        ResourceList<PipelineLayout, PipelineLayoutHandle> _pipelineLayoutList = {};

        SamplerHandle _defaultSampler = {};
        SamplerHandle _defaultShadowSampler = {};

        ImageHandle _defaultImage = {};

        std::vector<std::pair<Sampler::CreateInfo, std::unique_ptr<Sampler>>> _samplers = {};

        VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
        tsl::robin_map<CommandBuffer::DescriptorKey, std::pair<VkDescriptorSet, i32>, ende::util::MurmurHash<CommandBuffer::DescriptorKey>> _descriptorSets = {};

        tsl::robin_map<CommandBuffer::PipelineKey, VkPipeline, ende::util::MurmurHash<CommandBuffer::PipelineKey>, CommandBuffer::PipelineEqual> _pipelines = {};

        BufferHandle _markerBuffer[FRAMES_IN_FLIGHT] = {};
        u32 _offset = 0;
        u32 _marker = 1;
        std::vector<std::pair<std::string_view, u32>> _markedCmds[FRAMES_IN_FLIGHT] = {};

        u32 _bytesAllocatedPerFrame = 0;
        u32 _bytesUploadedToGPUPerFrame = 0;

        u32 _totalAllocated = 0;
        u32 _totalDeallocated = 0;

    };

}

#endif //CALA_DEVICE_H
