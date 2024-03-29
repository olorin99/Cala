#include <Cala/vulkan/Device.h>
#include <Cala/vulkan/primitives.h>
#include <Ende/filesystem/File.h>
#include <Ende/profile/profile.h>
#include <Cala/vulkan/ShaderModule.h>
#include <Cala/shaderBridge.h>

std::expected<std::unique_ptr<cala::vk::Device>, cala::vk::Error> cala::vk::Device::create(cala::vk::Device::CreateInfo info) {
    auto device = std::make_unique<Device>();
    if (info.logger)
        device->_logger = info.logger;

    std::vector<const char*> requiredExtensions;
    if (info.platform)
        requiredExtensions = info.platform->requiredExtensions();

    auto contextResult = Context::create({
        .applicationName = "CalaExample",
        .instanceExtensions = requiredExtensions,
        .logger = device->_logger,
        .requestedFeatures = {
                .meshShader = true
        }
    });
    if (!contextResult)
        return std::unexpected(contextResult.error());
    device->_context = std::move(contextResult.value());

    for (auto& frameCommandPool : device->_commandPools) {
        frameCommandPool = { CommandPool(device.get(), QueueType::GRAPHICS), CommandPool(device.get(), QueueType::COMPUTE), CommandPool(device.get(), QueueType::TRANSFER) };
    }

    device->_timelineSemaphore = Semaphore(device.get(), info.useTimeline ? 10 : -1);
    device->_immediateSemaphore = Semaphore(device.get(), info.useTimeline ? 1 : -1);

    if (info.useTimeline) {
        for (auto& value : device->_frameValues)
            value = 10;
    } else {
        for (auto& fence : device->_frameFences) {
            VkFenceCreateInfo fenceCreateInfo{};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence(device->context().device(), &fenceCreateInfo, nullptr, &fence);
        }
    }

    device->_immediateTimer._driver = device.get();

    VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, device->context().getLimits().maxBindlessSampledImages },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, device->context().getLimits().maxBindlessStorageBuffers },
            { VK_DESCRIPTOR_TYPE_SAMPLER, device->context().getLimits().maxBindlessSamplers },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, device->context().getLimits().maxBindlessStorageImages }
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolCreateInfo.maxSets = 1;
    poolCreateInfo.poolSizeCount = 4;
    poolCreateInfo.pPoolSizes = poolSizes;
    VK_TRY(vkCreateDescriptorPool(device->context().device(), &poolCreateInfo, nullptr, &device->_bindlessPool));
    device->_context.setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_POOL, (u64)device->_bindlessPool, "BindlessPool");

    VkDescriptorSetLayoutBinding bindlessLayoutBinding[4] = {};

    // images
    bindlessLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindlessLayoutBinding[0].descriptorCount = device->_context.getLimits().maxBindlessSampledImages;
    bindlessLayoutBinding[0].binding = CALA_BINDLESS_SAMPLED_IMAGE;
    bindlessLayoutBinding[0].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[0].pImmutableSamplers = nullptr;

    // buffers
    bindlessLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindlessLayoutBinding[1].descriptorCount = device->_context.getLimits().maxBindlessStorageBuffers;
    bindlessLayoutBinding[1].binding = CALA_BINDLESS_BUFFERS;
    bindlessLayoutBinding[1].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[1].pImmutableSamplers = nullptr;

    // samplers
    bindlessLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindlessLayoutBinding[2].descriptorCount = device->_context.getLimits().maxBindlessSamplers;
    bindlessLayoutBinding[2].binding = CALA_BINDLESS_SAMPLERS;
    bindlessLayoutBinding[2].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[2].pImmutableSamplers = nullptr;

    // storage images
    bindlessLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindlessLayoutBinding[3].descriptorCount = device->_context.getLimits().maxBindlessStorageImages;
    bindlessLayoutBinding[3].binding = CALA_BINDLESS_STORAGE_IMAGE;
    bindlessLayoutBinding[3].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[3].pImmutableSamplers = nullptr;



    VkDescriptorSetLayoutCreateInfo bindlessLayoutCreateInfo{};
    bindlessLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    bindlessLayoutCreateInfo.bindingCount = 4;
    bindlessLayoutCreateInfo.pBindings = bindlessLayoutBinding;
    bindlessLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    bindlessLayoutCreateInfo.pNext = nullptr;

    VkDescriptorBindingFlags bindingFlags[4] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{};
    extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    extendedInfo.bindingCount = 4;
    extendedInfo.pBindingFlags = bindingFlags;
    bindlessLayoutCreateInfo.pNext = &extendedInfo;

    VK_TRY(vkCreateDescriptorSetLayout(device->context().device(), &bindlessLayoutCreateInfo, nullptr, &device->_bindlessLayout));
    device->context().setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (u64)device->_bindlessLayout, "BindlessLayout");

    VkDescriptorSetAllocateInfo bindlessAllocate{};
    bindlessAllocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    bindlessAllocate.descriptorSetCount = 1;
    bindlessAllocate.pSetLayouts = &device->_bindlessLayout;
    bindlessAllocate.descriptorPool = device->_bindlessPool;

    VK_TRY(vkAllocateDescriptorSets(device->context().device(), &bindlessAllocate, &device->_bindlessSet));
    device->context().setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (u64)device->_bindlessSet, "BindlessSet");

    VkDescriptorPoolSize poolSizes2[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000}
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    descriptorPoolCreateInfo.poolSizeCount = 4;
    descriptorPoolCreateInfo.pPoolSizes = poolSizes2;
    descriptorPoolCreateInfo.maxSets = 10000;
    VK_TRY(vkCreateDescriptorPool(device->context().device(), &descriptorPoolCreateInfo, nullptr, &device->_descriptorPool));

#ifndef NDEBUG
    if (device->context().getSupportedExtensions().AMD_buffer_marker && device->context().getSupportedExtensions().AMD_device_coherent_memory) {
        u32 i = 0;
        for (auto& buffer : device->_markerBuffer) {
            buffer = device->createBuffer({
                .size = sizeof(u32) * 1000,
                .usage = BufferUsage::TRANSFER_DST,
                .memoryType = MemoryProperties::READBACK,
                .persistentlyMapped = true,
                .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                .name = "MarkerBuffer: " + std::to_string(i++)
            });
        }
    }
#endif

    device->_defaultSampler = device->getSampler({});
    device->_defaultShadowSampler = device->getSampler({
        .filter = VK_FILTER_NEAREST,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    });

    device->_defaultImage = device->createImage({
        .width = 2,
        .height = 2,
        .format = Format::RGBA8_UNORM,
        .usage = ImageUsage::SAMPLED | ImageUsage::STORAGE | ImageUsage::TRANSFER_DST,
        .name = "defaultImage"
    });
    device->immediate([&](CommandHandle cmd) {
        auto barrier = device->_defaultImage->barrier(PipelineStage::TOP, PipelineStage::COMPUTE_SHADER | PipelineStage::FRAGMENT_SHADER, Access::NONE, Access::SHADER_READ, ImageLayout::SHADER_READ_ONLY);
        cmd->pipelineBarrier({ &barrier, 1 });
    });

    return device;
}

cala::vk::Device::~Device() {
    if (_context.device() == VK_NULL_HANDLE) //TODO: fill out with entire early return set
        return;


    if (usingTimeline())
        _timelineSemaphore.signal(std::numeric_limits<u64>::max());
    VK_TRY(vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS))); //ensures last frame finished before destroying stuff

    _defaultImage = {};

    for (auto& markerBuffer : _markerBuffer)
        markerBuffer = BufferHandle{};

    for (auto& poolArray : _commandPools) {
        for (auto& pool : poolArray)
            pool.destroy();
    }

    _descriptorSets.clear();
    vkDestroyDescriptorSetLayout(_context.device(), _bindlessLayout, nullptr);

    vkDestroyDescriptorPool(_context.device(), _descriptorPool, nullptr);
    vkDestroyDescriptorPool(_context.device(), _bindlessPool, nullptr);

    _pipelineLayoutList.clearAll([](i32 index, PipelineLayout& layout) {
        layout = PipelineLayout(nullptr);
    });

    _shaderModulesList.clearAll([this](i32 index, ShaderModule& module) {
        if (module.module() != VK_NULL_HANDLE)
            vkDestroyShaderModule(context().device(), module.module(), nullptr);
    });


    for (auto& pipeline : _pipelines)
        vkDestroyPipeline(_context.device(), pipeline.second, nullptr);
    _pipelines.clear();

    _bufferList.clearAll([this](i32 index, Buffer& buffer) {
        buffer._mapped = Buffer::Mapped();
        VkBuffer buf = buffer._buffer;
        VmaAllocation allocation = buffer._allocation;
        if (allocation)
            vmaDestroyBuffer(context().allocator(), buf, allocation);
        buffer._allocation = nullptr;
    });

    _imageList.clearAll([this](i32 index, Image& image) {
        image._defaultView = Image::View();
        VmaAllocation allocation = image._allocation;
        if (image.image() != VK_NULL_HANDLE)
            vmaDestroyImage(context().allocator(), image.image(), allocation);
        image._image = VK_NULL_HANDLE;
        image._allocation = nullptr;
        image._width = 0;
        image._height = 0;
        image._depth = 0;
        image._layers = 0;
        image._mips = 0;
        image._format = Format::UNDEFINED;
    });

    clearFramebuffers();

    for (auto& renderPass : _renderPasses)
        delete renderPass.second;

    if (!usingTimeline()) {
        for (auto& fence : _frameFences)
            vkDestroyFence(_context.device(), fence, nullptr);
    }

    for (auto& setLayout : _setLayouts)
        vkDestroyDescriptorSetLayout(_context.device(), setLayout.second, nullptr);
}

cala::vk::Device::Device(cala::vk::Device &&rhs) noexcept {
    std::swap(_logger, rhs._logger);
    std::swap(_context, rhs._context);
    std::swap(_commandPools, rhs._commandPools);
    std::swap(_timelineSemaphore, rhs._timelineSemaphore);
    std::swap(_immediateSemaphore, rhs._immediateSemaphore);
    std::swap(_frameValues, rhs._frameValues);
    std::swap(_frameFences, rhs._frameFences);
    std::swap(_frameCount, rhs._frameCount);
    std::swap(_frameClock, rhs._frameClock);
    std::swap(_lastFrameTime, rhs._lastFrameTime);
    std::swap(_deferredCommands, rhs._deferredCommands);
    std::swap(_renderPasses, rhs._renderPasses);
    std::swap(_framebuffers, rhs._framebuffers);
    std::swap(_setLayouts, rhs._setLayouts);
    std::swap(_bindlessLayout, rhs._bindlessLayout);
    std::swap(_bindlessSet, rhs._bindlessSet);
    std::swap(_bindlessPool, rhs._bindlessPool);
    std::swap(_bindlessIndex, rhs._bindlessIndex);
    std::swap(_bufferList, rhs._bufferList);
    std::swap(_imageList, rhs._imageList);
    std::swap(_shaderModulesList, rhs._shaderModulesList);
    std::swap(_pipelineLayoutList, rhs._pipelineLayoutList);
    std::swap(_defaultSampler, rhs._defaultSampler);
    std::swap(_defaultShadowSampler, rhs._defaultShadowSampler);
    std::swap(_samplers, rhs._samplers);
    std::swap(_descriptorPool, rhs._descriptorPool);
    std::swap(_descriptorSets, rhs._descriptorSets);
    std::swap(_pipelines, rhs._pipelines);
    std::swap(_markerBuffer, rhs._markerBuffer);
    std::swap(_offset, rhs._offset);
    std::swap(_marker, rhs._marker);
    std::swap(_markedCmds, rhs._markedCmds);
    std::swap(_bytesAllocatedPerFrame, rhs._bytesAllocatedPerFrame);
    std::swap(_bytesUploadedToGPUPerFrame, rhs._bytesUploadedToGPUPerFrame);
    std::swap(_totalAllocated, rhs._totalAllocated);
    std::swap(_totalDeallocated, rhs._totalDeallocated);
    std::swap(_immediateSemaphore, rhs._immediateSemaphore);
}

cala::vk::Device &cala::vk::Device::operator==(cala::vk::Device &&rhs) noexcept {
    std::swap(_logger, rhs._logger);
    std::swap(_context, rhs._context);
    std::swap(_commandPools, rhs._commandPools);
    std::swap(_timelineSemaphore, rhs._timelineSemaphore);
    std::swap(_immediateSemaphore, rhs._immediateSemaphore);
    std::swap(_frameValues, rhs._frameValues);
    std::swap(_frameFences, rhs._frameFences);
    std::swap(_frameCount, rhs._frameCount);
    std::swap(_frameClock, rhs._frameClock);
    std::swap(_lastFrameTime, rhs._lastFrameTime);
    std::swap(_deferredCommands, rhs._deferredCommands);
    std::swap(_renderPasses, rhs._renderPasses);
    std::swap(_framebuffers, rhs._framebuffers);
    std::swap(_setLayouts, rhs._setLayouts);
    std::swap(_bindlessLayout, rhs._bindlessLayout);
    std::swap(_bindlessSet, rhs._bindlessSet);
    std::swap(_bindlessPool, rhs._bindlessPool);
    std::swap(_bindlessIndex, rhs._bindlessIndex);
    std::swap(_bufferList, rhs._bufferList);
    std::swap(_imageList, rhs._imageList);
    std::swap(_shaderModulesList, rhs._shaderModulesList);
    std::swap(_pipelineLayoutList, rhs._pipelineLayoutList);
    std::swap(_defaultSampler, rhs._defaultSampler);
    std::swap(_defaultShadowSampler, rhs._defaultShadowSampler);
    std::swap(_samplers, rhs._samplers);
    std::swap(_descriptorPool, rhs._descriptorPool);
    std::swap(_descriptorSets, rhs._descriptorSets);
    std::swap(_pipelines, rhs._pipelines);
    std::swap(_markerBuffer, rhs._markerBuffer);
    std::swap(_offset, rhs._offset);
    std::swap(_marker, rhs._marker);
    std::swap(_markedCmds, rhs._markedCmds);
    std::swap(_bytesAllocatedPerFrame, rhs._bytesAllocatedPerFrame);
    std::swap(_bytesUploadedToGPUPerFrame, rhs._bytesUploadedToGPUPerFrame);
    std::swap(_totalAllocated, rhs._totalAllocated);
    std::swap(_totalDeallocated, rhs._totalDeallocated);
    std::swap(_immediateSemaphore, rhs._immediateSemaphore);
    return *this;
}

std::expected<cala::vk::Device::FrameInfo, cala::vk::Error> cala::vk::Device::beginFrame() {
    _frameCount++;
    _bytesAllocatedPerFrame = 0;
    _bytesUploadedToGPUPerFrame = 0;

    auto waitResult = waitFrame(frameIndex());

#ifndef NDEBUG
    if (_markerBuffer[frameIndex()]) {
        std::memset(_markerBuffer[frameIndex()]->persistentMapping(), 0, _markerBuffer[frameIndex()]->size());
        _offset = 0;
        _marker = 1;
        _markedCmds[frameIndex()].clear();
    }
#endif

    vmaSetCurrentFrameIndex(_context.allocator(), _frameCount);

    for (auto& pool : _commandPools[frameIndex()])
        pool.reset();

    if (waitResult) {
        return FrameInfo{
                _frameCount,
                getCommandBuffer(frameIndex()),
                _frameFences[frameIndex()]
        };
    }
    return std::unexpected(waitResult.error());
}

std::chrono::high_resolution_clock::duration cala::vk::Device::endFrame() {
    _lastFrameTime = _frameClock.reset();
    return _lastFrameTime;
}

std::expected<void, cala::vk::Error> cala::vk::Device::waitFrame(u64 frame, u64 timeout) {
    PROFILE_NAMED("Device::waitFrame");
    if (usingTimeline()) {
        u64 waitValue = _frameValues[frame];
        return _timelineSemaphore.wait(waitValue, timeout);
    } else {
        VkFence fence = _frameFences[frame];
        auto res = vkWaitForFences(_context.device(), 1, &fence, true, timeout);
        if (res == VK_ERROR_DEVICE_LOST) {
            return std::unexpected(static_cast<Error>(res));
        }
        vkResetFences(_context.device(), 1, &fence);
    }
    return {};
}

bool cala::vk::Device::wait(u64 timeout) {
    return VK_SUCCESS == vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS));
}

cala::vk::CommandHandle cala::vk::Device::beginSingleTimeCommands(QueueType queueType) {

    auto buffer = getCommandBuffer(frameIndex(), queueType);
    buffer->begin();
    return buffer;
}

std::expected<void, cala::vk::Error> cala::vk::Device::endSingleTimeCommands(CommandHandle buffer) {
    if (_immediateSemaphore.isTimeline()) {
        u64 waitValue = _immediateSemaphore.value();
        u64 signalValue = _immediateSemaphore.increment();
        std::array<vk::CommandBuffer::SemaphoreSubmit, 1> wait({ { &_immediateSemaphore, waitValue } });
        std::array<vk::CommandBuffer::SemaphoreSubmit, 1> signal({ { &_immediateSemaphore, signalValue } });
        return buffer->submit(wait, signal).and_then([&] (auto result) {
            return _immediateSemaphore.wait(signalValue);
        });
    } else {
        VkFence fence; //TODO: dont create/destroy fence each time
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        VK_TRY(vkCreateFence(context().device(), &fenceCreateInfo, nullptr, &fence));
        return buffer->submit({}, {}, fence).and_then([&] (auto result) -> std::expected<void, Error> {
            auto res = vkWaitForFences(context().device(), 1, &fence, true, 1000000000);
            if (res == VK_SUCCESS)
                vkResetFences(context().device(), 1, &fence);
            else
                _logger->error("Failed waiting for immediate fence");
            vkDestroyFence(context().device(), fence, nullptr);
            if (res != VK_SUCCESS)
                return std::unexpected(static_cast<Error>(res));
            return {};
        });
    }
}

cala::vk::CommandHandle cala::vk::Device::getCommandBuffer(u32 frame, QueueType queueType) {
    assert(frame < FRAMES_IN_FLIGHT && queueType <= QueueType::TRANSFER);
    u32 index = 0;
    switch (queueType) {
        case QueueType::GRAPHICS:
            index = 0;
            break;
        case QueueType::COMPUTE:
            index = 1;
            break;
        case QueueType::TRANSFER:
            index = 2;
            break;
        default:
            index = 0;
    }
    return _commandPools[frame][index].getBuffer();
}


bool cala::vk::Device::gc() {
    PROFILE_NAMED("Device::gc");
    if (!_deferredCommands.empty()) {
        immediate([&](CommandHandle cmd) {
            for (auto& deferredCommand : _deferredCommands) {
                deferredCommand(cmd);
            }
        });
        _deferredCommands.clear();
    }

    _bufferList.clearDestroyQueue([this](i32 index, Buffer& buffer) {
        buffer._mapped = Buffer::Mapped();
        VkBuffer buf = buffer.buffer();
        VmaAllocation allocation = buffer._allocation;
        if (allocation)
            vmaDestroyBuffer(context().allocator(), buf, allocation);
        else
            _logger->warn("attempted to destroy buffer ({}) which has invalid allocation", index);
        buffer._allocation = nullptr;
        _totalDeallocated += buffer.size();
        if (buffer._debugName.empty())
            _logger->info("destroyed buffer at index ({})", index);
        else
            _logger->info("destroyed buffer at index ({}) named: {}", index, buffer.debugName());
        buffer = Buffer(this);
    });

    _imageList.clearDestroyQueue([this](i32 index, Image& image) {
        VmaAllocation allocation = image._allocation;
        image._defaultView = vk::Image::View();
        updateBindlessImage(index, _defaultImage->_defaultView);
        if (image.image() != VK_NULL_HANDLE)
            vmaDestroyImage(context().allocator(), image.image(), allocation);
        else
            _logger->warn("attempted to destroy image ({}) which is invalid", index);
        _totalDeallocated += image.width() * image.height() * image.depth() * formatToSize(image.format());
        image._image = VK_NULL_HANDLE;
        image._allocation = nullptr;
        image._width = 0;
        image._height = 0;
        image._depth = 0;
        image._layers = 0;
        image._mips = 0;
        image._format = Format::UNDEFINED;
        if (image._debugName.empty())
            _logger->info("destroyed image at index ({})", index);
        else
            _logger->info("destroyed image at index ({}) named: {}", index, image.debugName());
    });

    _shaderModulesList.clearDestroyQueue([this](i32 index, ShaderModule& module) {
        vkDestroyShaderModule(context().device(), module.module(), nullptr);
        module._module = VK_NULL_HANDLE;
        _logger->info("destroyed shader module ({})", index);
    });

    _pipelineLayoutList.clearDestroyQueue([this](i32 index, PipelineLayout& layout) {
        layout = PipelineLayout(nullptr);
        _logger->info("destroyed pipeline layout ({})", index);
    });

    for (auto it = _descriptorSets.begin(); it != _descriptorSets.end(); it++) {
        auto& frame = it.value().second;
        if (frame < 0) {
            vkFreeDescriptorSets(_context.device(), _descriptorPool, 1, &it.value().first);
            _descriptorSets.erase(it);
        } else
            --frame;
    }

    for (auto it = _framebuffers.begin(); it != _framebuffers.end(); it++) {
        auto& frame = it.value().first;
        if (frame < 0) {
            delete it.value().second;
            _framebuffers.erase(it);
        } else
            --frame;
    }
    return true;
}

cala::vk::BufferHandle cala::vk::Device::createBuffer(cala::vk::Device::BufferInfo info) {
//    u32 index = 0;
    info.usage = info.usage | BufferUsage::TRANSFER_DST | BufferUsage::TRANSFER_SRC | BufferUsage::STORAGE | BufferUsage::DEVICE_ADDRESS;

    VkBuffer buffer;
    VmaAllocation allocation;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = info.size;
    bufferInfo.usage = getBufferUsage(info.usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
//    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.requiredFlags = info.requiredFlags;
    allocInfo.preferredFlags = info.preferredFlags;

    if ((info.memoryType & MemoryProperties::DEVICE) == MemoryProperties::DEVICE) {
//        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        info.persistentlyMapped = false;
    }
    if ((info.memoryType & MemoryProperties::STAGING) == MemoryProperties::STAGING) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    if ((info.memoryType & MemoryProperties::READBACK) == MemoryProperties::READBACK) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    VK_TRY(vmaCreateBuffer(context().allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr));

    _totalAllocated += info.size;

    i32 index = _bufferList.insert(this);

    _bufferList.getResource(index)->_buffer = buffer;
    if (_context.getEnabledFeatures().deviceAddress) {
        VkBufferDeviceAddressInfo deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        deviceAddressInfo.buffer = buffer;
        _bufferList.getResource(index)->_address = vkGetBufferDeviceAddress(_context.device(), &deviceAddressInfo);
    }
    _bufferList.getResource(index)->_allocation = allocation;
    _bufferList.getResource(index)->_size = info.size;
    _bufferList.getResource(index)->_usage = info.usage;
    _bufferList.getResource(index)->_flags = info.memoryType;
    _bufferList.getResource(index)->_debugName = info.name;
    if (info.persistentlyMapped)
        _bufferList.getResource(index)->_mapped = _bufferList.getResource(index)->map();


    _bytesAllocatedPerFrame += info.size;

    updateBindlessBuffer(index);

    if (info.name.empty())
        _context.setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)buffer, "Buffer: " + std::to_string(index));
    else
        _context.setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)buffer, info.name);

    return _bufferList.getHandle(this, index);
}

cala::vk::BufferHandle cala::vk::Device::resizeBuffer(BufferHandle handle, u32 size, bool transfer) {
    assert(handle);
    auto newHandle = createBuffer({
        .size = size,
        .usage = handle->usage(),
        .memoryType = handle->flags(),
        .persistentlyMapped = handle->persistentlyMapped(),
        .name = handle->debugName()
    });
    assert(newHandle);
    if (transfer) {
        immediate([&](vk::CommandHandle cmd) {
            VkBufferCopy bufferCopy{};
            bufferCopy.dstOffset = 0;
            bufferCopy.srcOffset = 0;
            bufferCopy.size = handle->size();
            vkCmdCopyBuffer(cmd->buffer(), handle->buffer(), newHandle->buffer(), 1, &bufferCopy);
        });
    }
    return newHandle;
}

cala::vk::ImageHandle cala::vk::Device::createImage(Image::CreateInfo info) {
    VkImage image;
    VmaAllocation allocation;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    auto type = info.type;
    if (info.type == ImageType::AUTO) {
        if (info.depth > 1) {
            imageInfo.imageType = VK_IMAGE_TYPE_3D;
            type = ImageType::IMAGE3D;
        }
        else if (info.height > 1) {
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            type = ImageType::IMAGE2D;
        }
        else {
            imageInfo.imageType = VK_IMAGE_TYPE_1D;
            type = ImageType::IMAGE1D;
        }
    } else {
        imageInfo.imageType = getImageType(info.type);
    }
    imageInfo.format = getFormat(info.format);
    imageInfo.extent.width = info.width;
    imageInfo.extent.height = info.height;
    imageInfo.extent.depth = info.depth;
    imageInfo.mipLevels = info.mipLevels;
    imageInfo.arrayLayers = info.arrayLayers;

    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = getImageUsage(info.usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (info.aliasFormat != Format::UNDEFINED) {
        imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        VkImageFormatListCreateInfo listCreateInfo{};
        listCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
        listCreateInfo.viewFormatCount = 1;
        VkFormat aliasFormat = getFormat(info.aliasFormat);
        listCreateInfo.pViewFormats = &aliasFormat;
        imageInfo.pNext = &listCreateInfo;
    }

    if (imageInfo.arrayLayers == 6)
        imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = 0;

    VK_TRY(vmaCreateImage(context().allocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr));

    _totalAllocated += info.width * info.height * info.depth * formatToSize(info.format);

    i32 index = _imageList.insert(this);

    _imageList.getResource(index)->_image = image;
    _imageList.getResource(index)->_allocation = allocation;
    _imageList.getResource(index)->_width = info.width;
    _imageList.getResource(index)->_height = info.height;
    _imageList.getResource(index)->_depth = info.depth;
    _imageList.getResource(index)->_layers = info.arrayLayers;
    _imageList.getResource(index)->_mips = info.mipLevels;
    _imageList.getResource(index)->_format = info.format;
    _imageList.getResource(index)->_usage = info.usage;
    _imageList.getResource(index)->_type = type;
    _imageList.getResource(index)->_debugName = info.name;

    _imageList.getResource(index)->_defaultView = std::move(_imageList.getResource(index)->newView(0, _imageList.getResource(index)->mips()));

    bool isSampled = (info.usage & ImageUsage::SAMPLED) == ImageUsage::SAMPLED;
    bool isStorage = (info.usage & ImageUsage::STORAGE) == ImageUsage::STORAGE;
    updateBindlessImage(index, _imageList.getResource(index)->_defaultView, isSampled, isStorage);

    _bytesAllocatedPerFrame += (info.width * info.height * info.depth * formatToSize(info.format));

    if (info.name.empty()) {
        context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)image, "Image: " + std::to_string(index));
        context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_imageList.getResource(index)->_defaultView.view, "ImageView: " + std::to_string(index));
    } else {
        _context.setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)image, info.name);
        context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_imageList.getResource(index)->_defaultView.view, info.name + "_View");
    }

    return _imageList.getHandle(this, index);
}

cala::vk::ImageHandle cala::vk::Device::getImageHandle(u32 index) {
    assert(index < _imageList.allocated());
    return _imageList.getHandle(this, index);
}

cala::vk::ShaderModuleHandle cala::vk::Device::createShaderModule(std::span<u32> spirv, ShaderStage stage) {
    VkShaderModule module;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(u32);
    createInfo.pCode = spirv.data();

    if (vkCreateShaderModule(context().device(), &createInfo, nullptr, &module) != VK_SUCCESS)
        return {};

    ShaderModuleInterface interface(spirv, stage);

    i32 index = _shaderModulesList.insert(this);
    _shaderModulesList.getResource(index)->_module = module;
    _shaderModulesList.getResource(index)->_stage = stage;
    _shaderModulesList.getResource(index)->_main = "main";
    _shaderModulesList.getResource(index)->_localSize = interface._localSize;
    _shaderModulesList.getResource(index)->_interface = std::move(interface);
    return _shaderModulesList.getHandle(this, index);
}

cala::vk::ShaderModuleHandle cala::vk::Device::recreateShaderModule(ShaderModuleHandle handle, std::span<u32> spirv, cala::vk::ShaderStage stage) {
    if (!handle)
        return createShaderModule(spirv, stage);
    i32 handleIndex = handle.index();
    VkShaderModule module;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(u32);
    createInfo.pCode = spirv.data();

    if (vkCreateShaderModule(context().device(), &createInfo, nullptr, &module) != VK_SUCCESS)
        return {};

    ShaderModuleInterface interface(spirv, stage);

    i32 index = _shaderModulesList.insert(this);

    _shaderModulesList._resources[index].second.swap(_shaderModulesList._resources[handleIndex].second);
    _shaderModulesList._resources[index].second->index = index;
    _shaderModulesList._resources[handleIndex].second->index = handleIndex;

//    _shaderModulesList._resources[index].second->count = _shaderModulesList._resources[handleIndex].second->count;
    _shaderModulesList._resources[handleIndex].second->count = 0;
    _shaderModulesList._resources[handleIndex].second->deleter(handleIndex);

    _shaderModulesList.getResource(index)->_module = module;
    _shaderModulesList.getResource(index)->_stage = stage;
    _shaderModulesList.getResource(index)->_main = "main";
    _shaderModulesList.getResource(index)->_localSize = interface._localSize;
    _shaderModulesList.getResource(index)->_interface = std::move(interface);
    return _shaderModulesList.getHandle(this, index);
}

cala::vk::PipelineLayoutHandle cala::vk::Device::createPipelineLayout(const cala::vk::ShaderInterface &interface) {
    PipelineLayout layout(this, interface);

    i32 index = _pipelineLayoutList.insert(this);
    *_pipelineLayoutList._resources[index].first = std::move(layout);
    return _pipelineLayoutList.getHandle(this, index);
}

cala::vk::PipelineLayoutHandle cala::vk::Device::recreatePipelineLayout(cala::vk::PipelineLayoutHandle handle, const cala::vk::ShaderInterface &interface) {
    if (!handle)
        return createPipelineLayout(interface);
    i32 handleIndex = handle.index();
    PipelineLayout layout(this, interface);

    i32 index = _pipelineLayoutList.insert(this);
    _pipelineLayoutList._resources[index].second.swap(_pipelineLayoutList._resources[handleIndex].second);
    _pipelineLayoutList._resources[index].second->index = index;
    _pipelineLayoutList._resources[handleIndex].second->index = handleIndex;
    _pipelineLayoutList._resources[handleIndex].second->count = handleIndex;
    _pipelineLayoutList._resources[handleIndex].second->deleter(handleIndex);

    *_pipelineLayoutList._resources[index].first = std::move(layout);
    return _pipelineLayoutList.getHandle(this, index);
}


cala::vk::SamplerHandle cala::vk::Device::getSampler(Sampler::CreateInfo info) {
    for (i32 index = 0; index < _samplers.size(); index++) {
        if (_samplers[index].first == info)
            return { this, index, nullptr };
    }

    u32 index = _samplers.size();
    _samplers.emplace_back(std::make_pair(info, std::make_unique<Sampler>(*this, info)));
    updateBindlessSampler(index);
    return { this, static_cast<i32>(index), nullptr };
}




VkDescriptorSetLayout cala::vk::Device::getSetLayout(std::span<VkDescriptorSetLayoutBinding> bindings) {
    SetLayoutKey key{};
    for (u32 i = 0; i < bindings.size(); i++) {
        key.bindings[i] = bindings[i];
    }

    auto it = _setLayouts.find(key);
    if (it != _setLayouts.end()) {
        return it->second;
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();

    VkDescriptorSetLayout setLayout;
    VK_TRY(vkCreateDescriptorSetLayout(_context.device(), &createInfo, nullptr, &setLayout));

    _setLayouts.emplace(std::make_pair(key, setLayout));
    return setLayout;
}

void cala::vk::Device::updateBindlessBuffer(u32 index) {
    VkWriteDescriptorSet descriptorWrite{};
    VkDescriptorBufferInfo bufferInfo{};

//    auto buffer = _buffers[index].first.get();
    auto buffer = _bufferList.getResource(index);

    bufferInfo.buffer = buffer->buffer();
    bufferInfo.offset = 0;
    bufferInfo.range = buffer->size();

    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.dstArrayElement = index;
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = CALA_BINDLESS_BUFFERS;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(_context.device(), 1, &descriptorWrite, 0, nullptr);
}

void cala::vk::Device::updateBindlessImage(u32 index, Image::View &image, bool sampled, bool storage) {
    VkWriteDescriptorSet descriptorWrite[2] = { {}, {} };
    i32 writeNum = 0;

    VkDescriptorImageInfo sampledImageInfo{};
    if (sampled) {
        sampledImageInfo.imageView = image.view;
        sampledImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        descriptorWrite[writeNum].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[writeNum].descriptorCount = 1;
        descriptorWrite[writeNum].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrite[writeNum].dstArrayElement = index;
        descriptorWrite[writeNum].dstSet = _bindlessSet;
        descriptorWrite[writeNum].dstBinding = CALA_BINDLESS_SAMPLED_IMAGE;
        descriptorWrite[writeNum].pImageInfo = &sampledImageInfo;
        writeNum++;
    }

    VkDescriptorImageInfo storageImageInfo{};
    if (storage) {
        storageImageInfo.imageView = image.view;
        storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        descriptorWrite[writeNum].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[writeNum].descriptorCount = 1;
        descriptorWrite[writeNum].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrite[writeNum].dstArrayElement = index;
        descriptorWrite[writeNum].dstSet = _bindlessSet;
        descriptorWrite[writeNum].dstBinding = CALA_BINDLESS_STORAGE_IMAGE;
        descriptorWrite[writeNum].pImageInfo = &storageImageInfo;
        writeNum++;
    }

    vkUpdateDescriptorSets(_context.device(), writeNum, descriptorWrite, 0, nullptr);
}

void cala::vk::Device::updateBindlessSampler(u32 index) {
    VkWriteDescriptorSet  descriptorWrite{};
    VkDescriptorImageInfo imageInfo{};

    imageInfo.sampler = _samplers[index].second->sampler();

    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrite.dstArrayElement = index;
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = CALA_BINDLESS_SAMPLERS;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_context.device(), 1, &descriptorWrite, 0, nullptr);
}

void cala::vk::Device::setBindlessSetIndex(u32 index) {
//    for (auto& cmd : _frameCommands)
//        cmd.setBindlessIndex(index);
    _bindlessIndex = index;
}

cala::vk::RenderPass* cala::vk::Device::getRenderPass(std::span<RenderPass::Attachment> attachments) {
//    u64 hash = ende::util::murmur3(reinterpret_cast<u32*>(attachments.data()), attachments.size() * sizeof(RenderPass::Attachment), attachments.size());
    u64 hash = 0;
    for (auto& attachment : attachments) {
        hash = ende::util::combineHash(hash, (u64)attachment.format * 0x100000001b3ull);
        hash = ende::util::combineHash(hash, (u64)attachment.internalLayout << 40);
        hash = ende::util::combineHash(hash, (u64)attachment.loadOp << 35);
        hash = ende::util::combineHash(hash, (u64)attachment.storeOp << 30);
        hash = ende::util::combineHash(hash, (u64)attachment.finalLayout << 20);
        hash = ende::util::combineHash(hash, (u64)attachment.initialLayout << 10);
    }

    auto it = _renderPasses.find(hash);
    if (it != _renderPasses.end())
        return it.value();

    auto a = _renderPasses.emplace(std::make_pair(hash, new RenderPass(*this, attachments)));
    return a.first.value();
}

cala::vk::Framebuffer *cala::vk::Device::getFramebuffer(RenderPass *renderPass, std::span<VkImageView> attachmentImages, std::span<u32> hashes, u32 width, u32 height) {
    u64 hash = ((u64)renderPass->id() << 32);
    for (auto& attachment : hashes)
        hash = ende::util::combineHash(hash, (u64)attachment);

    auto it = _framebuffers.find(hash);
    if (it != _framebuffers.end()) {
        it.value().first = FRAMES_IN_FLIGHT + 1;
        return it.value().second;
    }

    auto a = _framebuffers.emplace(std::make_pair(hash, std::make_pair(FRAMES_IN_FLIGHT + 1, new Framebuffer(_context.device(), *renderPass, attachmentImages, width, height))));
    return a.first.value().second;
}

void cala::vk::Device::clearFramebuffers() {
    for (auto& framebuffer : _framebuffers)
        delete framebuffer.second.second;
    _framebuffers.clear();
}

VkDescriptorSet cala::vk::Device::getDescriptorSet(CommandBuffer::DescriptorKey key) {
    PROFILE_NAMED("Device::getDescriptorSet");

    if (key.setLayout == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    auto it = _descriptorSets.find(key);
    if (it != _descriptorSets.end()) {
        it.value().second = FRAMES_IN_FLIGHT + 1;
        return it->second.first;
    }


    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &key.setLayout;

    VkDescriptorSet descriptorSet;
    VK_TRY(vkAllocateDescriptorSets(_context.device(), &allocInfo, &descriptorSet));

    for (u32 i = 0; i < MAX_BINDING_PER_SET; i++) {

        if (key.buffers[i].buffer) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = key.buffers[i].buffer->buffer();
            bufferInfo.offset = key.buffers[i].offset;
            bufferInfo.range = key.buffers[i].range;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = i;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = key.buffers[i].storage ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;

            //TODO: batch writes
            vkUpdateDescriptorSets(context().device(), 1, &descriptorWrite, 0, nullptr);
        } else if (key.images[i].image != nullptr) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = key.images[i].storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = key.images[i].view;
            imageInfo.sampler = key.images[i].sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = i;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = key.images[i].storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = nullptr;
            descriptorWrite.pImageInfo = &imageInfo;
            descriptorWrite.pTexelBufferView = nullptr;

            //TODO: batch writes
            vkUpdateDescriptorSets(context().device(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    _descriptorSets.emplace(std::make_pair(key, std::make_pair(descriptorSet, FRAMES_IN_FLIGHT + 1)));
    return descriptorSet;
}

std::expected<VkPipeline, cala::vk::Error> cala::vk::Device::getPipeline(CommandBuffer::PipelineKey key) {
    PROFILE_NAMED("Device::getPipeline");
    // check if exists in cache
    auto it = _pipelines.find(key);
    if (it != _pipelines.end())
        return it->second;

    // create new pipeline
    VkPipeline pipeline;
    if (!key.compute) {

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        pipelineInfo.stageCount = key.shaderCount;
        pipelineInfo.pStages = key.shaders;

        assert(key.framebuffer);
        pipelineInfo.renderPass = key.framebuffer->renderPass().renderPass();
        pipelineInfo.subpass = 0;

        u32 countVertexBinding = 0;
        u32 countVertexAttribute = 0;
        for (u32 binding = 0; binding < key.bindingCount; binding++) {
            if (key.bindings[binding].stride > 0)
                countVertexBinding++;
        }
        for (u32 attribute = 0; attribute < key.attributeCount; attribute++) {
            if (key.attributes[attribute].format > 0)
                countVertexAttribute++;
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = key.bindingCount;
        vertexInputInfo.pVertexBindingDescriptions = key.bindings;
        vertexInputInfo.vertexAttributeDescriptionCount = key.attributeCount;
        vertexInputInfo.pVertexAttributeDescriptions = key.attributes;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        auto frameSize = key.framebuffer->extent();

        VkViewport viewport{};

        //default viewport
        if (key.viewPort.x == 0 &&
                key.viewPort.y == 0 &&
                key.viewPort.width == 0 &&
                key.viewPort.height == 0 &&
                key.viewPort.minDepth == 0.f &&
                key.viewPort.maxDepth == 1.f) {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = frameSize.first;
            viewport.height = frameSize.second;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        } else { //custom viewport
            viewport.x = key.viewPort.x;
            viewport.y = key.viewPort.y;
            viewport.width = key.viewPort.width;
            viewport.height = key.viewPort.height;
            viewport.minDepth = key.viewPort.minDepth;
            viewport.maxDepth = key.viewPort.maxDepth;
        }

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {.width=frameSize.first, .height=frameSize.second};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = key.raster.depthClamp;
        rasterizer.rasterizerDiscardEnable = key.raster.rasterDiscard;
        rasterizer.polygonMode = getPolygonMode(key.raster.polygonMode);
        rasterizer.lineWidth = key.raster.lineWidth;
        rasterizer.cullMode = getCullMode(key.raster.cullMode);
        rasterizer.frontFace = getFrontFace(key.raster.frontFace);
        rasterizer.depthBiasEnable = key.raster.depthBias;
        rasterizer.depthBiasConstantFactor = 0.f;
        rasterizer.depthBiasClamp = 0.f;
        rasterizer.depthBiasSlopeFactor = 0.f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.sampleShadingEnable = VK_FALSE;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.minSampleShading = 1.f;
        multisample.pSampleMask = nullptr;
        multisample.alphaToCoverageEnable = VK_FALSE;
        multisample.alphaToOneEnable = VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = key.depth.test;
        depthStencil.depthWriteEnable = key.depth.write;
        depthStencil.depthCompareOp = getCompareOp(key.depth.compareOp);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.f;
        depthStencil.maxDepthBounds = 1.f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        //TODO: set up so can be configured by user
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = key.blend.blend;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = getBlendFactor(key.blend.srcFactor);
        colorBlendAttachment.dstColorBlendFactor = getBlendFactor(key.blend.dstFactor);
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

        VkPipelineColorBlendAttachmentState colourBlendAttachments[3] {};
        for (auto& attachment : colourBlendAttachments)
            attachment = colorBlendAttachment;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = key.framebuffer->renderPass().colourAttachmentCount();
        colorBlending.pAttachments = colourBlendAttachments;
        colorBlending.blendConstants[0] = 0.f;
        colorBlending.blendConstants[1] = 0.f;
        colorBlending.blendConstants[2] = 0.f;
        colorBlending.blendConstants[3] = 0.f;

        if (key.shaders[0].stage != (VkShaderStageFlagBits)ShaderStage::TASK && key.shaders[0].stage != (VkShaderStageFlagBits)ShaderStage::MESH) {
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
        }
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = key.layout;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        auto res = vkCreateGraphicsPipelines(context().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        if (res != VK_SUCCESS)
            return std::unexpected(static_cast<Error>(res));
    } else {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = key.shaders[0];
        pipelineInfo.layout = key.layout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        auto res = vkCreateComputePipelines(context().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        if (res != VK_SUCCESS)
            return std::unexpected(static_cast<Error>(res));
    }

    _pipelines.emplace(std::make_pair(key, pipeline));

    return pipeline;
}

cala::vk::Device::Stats cala::vk::Device::stats() const {
    u32 allocatedBuffers = _bufferList.allocated();
    u32 buffersInUse = _bufferList.used();
    u32 allocatedImages = _imageList.allocated();
    u32 imagesInUse = _imageList.used();
    u32 descriptorSetCount = _descriptorSets.size();
    u32 pipelineCount = _pipelines.size();
    return {
        buffersInUse,
        allocatedBuffers,
        imagesInUse,
        allocatedImages,
        _shaderModulesList.used(),
        _shaderModulesList.allocated(),
        _pipelineLayoutList.used(),
        _pipelineLayoutList.allocated(),
        descriptorSetCount,
        pipelineCount,
        _bytesAllocatedPerFrame,
        _bytesUploadedToGPUPerFrame,
        _totalAllocated,
        _totalDeallocated
    };
}

void cala::vk::Device::printMarkers() {
    for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; frame++) {
        if (!_markerBuffer[frame])
            continue;

        _logger->warn("Frame: {} Marked Commands", frame);

        u32* markers = static_cast<u32*>(_markerBuffer[frame]->persistentMapping());
        for (u32 i = 0; i < _markerBuffer[frame]->size() / sizeof(u32); i++) {
            u32 marker = markers[i];
            auto cmd = i < _markedCmds[frame].size() ? _markedCmds[frame][i] : std::make_pair( "NullCmd", 0 );
            _logger->warn("Command: {}, Marker[{}]: {}", cmd.first, i, marker);
            if (marker == 0)
                break;
        }
    }
}