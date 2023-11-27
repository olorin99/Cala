#include "Cala/backend/vulkan/Device.h"
#include "Cala/backend/vulkan/primitives.h"
#include "Ende/filesystem/File.h"
#include <Ende/profile/profile.h>
#include <Cala/backend/vulkan/ShaderModule.h>

cala::backend::vulkan::Device::Device(cala::backend::Platform& platform, spdlog::logger& logger, CreateInfo createInfo)
    : _logger(logger),
      _context(this, platform),
      _commandPools{
              {CommandPool(this, QueueType::GRAPHICS), CommandPool(this, QueueType::COMPUTE), CommandPool(this, QueueType::TRANSFER)},
              {CommandPool(this, QueueType::GRAPHICS), CommandPool(this, QueueType::COMPUTE), CommandPool(this, QueueType::TRANSFER)}
        },
      _frameCount(0),
      _bindlessIndex(-1),
      _timelineSemaphore(this, createInfo.useTimeline ? 10 : -1),
      _immediateSemaphore(this, createInfo.useTimeline ? 1 : -1),
      _bytesAllocatedPerFrame(0),
      _bytesUploadedToGPUPerFrame(0)
{
    if (createInfo.useTimeline) {
        for (auto& value : _frameValues)
            value = 10;
    } else {
        for (auto& fence : _frameFences) {
            VkFenceCreateInfo fenceCreateInfo{};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence(_context.device(), &fenceCreateInfo, nullptr, &fence);
        }
    }

    VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, _context.getLimits().maxBindlessSampledImages - 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _context.getLimits().maxBindlessStorageBuffers - 100 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, _context.getLimits().maxBindlessSamplers - 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _context.getLimits().maxBindlessStorageImages - 100 }
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolCreateInfo.maxSets = 1;
    poolCreateInfo.poolSizeCount = 4;
    poolCreateInfo.pPoolSizes = poolSizes;
    VK_TRY(vkCreateDescriptorPool(_context.device(), &poolCreateInfo, nullptr, &_bindlessPool));
    _context.setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_POOL, (u64)_bindlessPool, "BindlessPool");

    VkDescriptorSetLayoutBinding bindlessLayoutBinding[4] = {};

    // images
    bindlessLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindlessLayoutBinding[0].descriptorCount = _context.getLimits().maxBindlessSampledImages - 100;
    bindlessLayoutBinding[0].binding = 0;
    bindlessLayoutBinding[0].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[0].pImmutableSamplers = nullptr;

    // buffers
    bindlessLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindlessLayoutBinding[1].descriptorCount = _context.getLimits().maxBindlessStorageBuffers - 100;
    bindlessLayoutBinding[1].binding = 1;
    bindlessLayoutBinding[1].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[1].pImmutableSamplers = nullptr;

    // samplers
    bindlessLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindlessLayoutBinding[2].descriptorCount = _context.getLimits().maxBindlessSamplers - 100;
    bindlessLayoutBinding[2].binding = 2;
    bindlessLayoutBinding[2].stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding[2].pImmutableSamplers = nullptr;

    // storage images
    bindlessLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindlessLayoutBinding[3].descriptorCount = _context.getLimits().maxBindlessStorageImages - 100;
    bindlessLayoutBinding[3].binding = 3;
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

    VK_TRY(vkCreateDescriptorSetLayout(_context.device(), &bindlessLayoutCreateInfo, nullptr, &_bindlessLayout));
    _context.setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (u64)_bindlessLayout, "BindlessLayout");

    VkDescriptorSetAllocateInfo bindlessAllocate{};
    bindlessAllocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    bindlessAllocate.descriptorSetCount = 1;
    bindlessAllocate.pSetLayouts = &_bindlessLayout;
    bindlessAllocate.descriptorPool = _bindlessPool;

    VK_TRY(vkAllocateDescriptorSets(_context.device(), &bindlessAllocate, &_bindlessSet));
    _context.setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (u64)_bindlessSet, "BindlessSet");

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
    VK_TRY(vkCreateDescriptorPool(context().device(), &descriptorPoolCreateInfo, nullptr, &_descriptorPool));

#ifndef NDEBUG
    if (_context.getSupportedExtensions().AMD_buffer_marker && _context.getSupportedExtensions().AMD_device_coherent_memory) {
        u32 i = 0;
        for (auto& buffer : _markerBuffer) {
            buffer = createBuffer(sizeof(u32) * 1000, BufferUsage::TRANSFER_DST, MemoryProperties::READBACK, true, {
                    .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
            });
            std::string debugLabel = "MarkerBuffer: " + std::to_string(i++);
            _context.setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)buffer->buffer(), debugLabel);
        }
    }
#endif

    _defaultSampler = getSampler({});
    _defaultShadowSampler = getSampler({
        .filter = VK_FILTER_NEAREST,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    });
}

cala::backend::vulkan::Device::~Device() {
    if (usingTimeline())
        _timelineSemaphore.signal(std::numeric_limits<u64>::max());
    VK_TRY(vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS))); //ensures last frame finished before destroying stuff

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

//    _imageViews.clear();
    for (auto& view : _imageViews)
        view = Image::View();
    _imageList.clearAll([this](i32 index, Image& image) {
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


cala::backend::vulkan::Device::FrameInfo cala::backend::vulkan::Device::beginFrame() {
    _frameCount++;
    _bytesAllocatedPerFrame = 0;
    _bytesUploadedToGPUPerFrame = 0;

    if (!waitFrame(frameIndex())) {
        printMarkers();
        _logger.error("Error waiting for frame ({}), index ({})", _frameCount, frameIndex());
        return {
            _frameCount,
            {},
            _frameFences[frameIndex()]
        };
    }

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

    return {
        _frameCount,
        getCommandBuffer(frameIndex()),
        _frameFences[frameIndex()]
    };
}

ende::time::Duration cala::backend::vulkan::Device::endFrame() {
    _lastFrameTime = _frameClock.reset();
    return _lastFrameTime;
}

bool cala::backend::vulkan::Device::waitFrame(u64 frame, u64 timeout) {
    PROFILE_NAMED("Device::waitFrame");
    if (usingTimeline()) {
        u64 waitValue = _frameValues[frame];
        return _timelineSemaphore.wait(waitValue, timeout);
    } else {
        VkFence fence = _frameFences[frame];
        auto res = vkWaitForFences(_context.device(), 1, &fence, true, timeout);
        if (res == VK_ERROR_DEVICE_LOST) {
            _logger.error("Device lost on wait for fence");
            printMarkers();
            throw std::runtime_error("Device lost on wait for fence");
        }
        vkResetFences(_context.device(), 1, &fence);
        return true;
    }
}

bool cala::backend::vulkan::Device::wait(u64 timeout) {
    return VK_SUCCESS == vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS));
}

cala::backend::vulkan::BufferHandle cala::backend::vulkan::Device::stagingBuffer(u32 size) {
    return createBuffer(size, BufferUsage::TRANSFER_SRC, MemoryProperties::STAGING);
}


cala::backend::vulkan::CommandHandle cala::backend::vulkan::Device::beginSingleTimeCommands(QueueType queueType) {

    auto buffer = getCommandBuffer(frameIndex(), queueType);
    buffer->begin();
    return buffer;
}

void cala::backend::vulkan::Device::endSingleTimeCommands(CommandHandle buffer) {
    if (_immediateSemaphore.isTimeline()) {
        u64 waitValue = _immediateSemaphore.value();
        u64 signalValue = _immediateSemaphore.increment();
        std::array<backend::vulkan::CommandBuffer::SemaphoreSubmit, 1> wait({ { &_immediateSemaphore, waitValue } });
        std::array<backend::vulkan::CommandBuffer::SemaphoreSubmit, 1> signal({ { &_immediateSemaphore, signalValue } });
        if (!buffer->submit(wait, signal)) {
            _logger.error("Error submitting command buffer");
            printMarkers();
            throw std::runtime_error("Error submitting immediate command buffer");
        }
        _immediateSemaphore.wait(signalValue);
    } else {
        VkFence fence; //TODO: dont create/destroy fence each time
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        VK_TRY(vkCreateFence(context().device(), &fenceCreateInfo, nullptr, &fence));
        if (!buffer->submit({}, {}, fence)) {
            _logger.error("Error submitting command buffer");
            printMarkers();
            throw std::runtime_error("Error submitting immediate command buffer");
        }

        auto res = vkWaitForFences(context().device(), 1, &fence, true, 1000000000) == VK_SUCCESS;
        if (res)
            vkResetFences(context().device(), 1, &fence);
        else
            _logger.error("Failed waiting for immediate fence");
        vkDestroyFence(context().device(), fence, nullptr);
    }
}

cala::backend::vulkan::CommandHandle cala::backend::vulkan::Device::getCommandBuffer(u32 frame, QueueType queueType) {
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


bool cala::backend::vulkan::Device::gc() {
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
            _logger.warn("attempted to destroy buffer ({}) which has invalid allocation", index);
        buffer._allocation = nullptr;
        _totalDeallocated += buffer.size();
        buffer = Buffer(this);
        _logger.info("destroyed buffer ({})", index);
    });

    _imageList.clearDestroyQueue([this](i32 index, Image& image) {
        VmaAllocation allocation = image._allocation;
        _imageViews[index] = backend::vulkan::Image::View();
        updateBindlessImage(index, _imageViews[0]);
        if (image.image() != VK_NULL_HANDLE)
            vmaDestroyImage(context().allocator(), image.image(), allocation);
        else
            _logger.warn("attempted to destroy image ({}) which is invalid", index);
        _totalDeallocated += image.width() * image.height() * image.depth() * formatToSize(image.format());
        image._image = VK_NULL_HANDLE;
        image._allocation = nullptr;
        image._width = 0;
        image._height = 0;
        image._depth = 0;
        image._layers = 0;
        image._mips = 0;
        image._format = Format::UNDEFINED;
        _logger.info("destroyed image ({})", index);
    });

    _shaderModulesList.clearDestroyQueue([this](i32 index, ShaderModule& module) {
        vkDestroyShaderModule(context().device(), module.module(), nullptr);
        module._module = VK_NULL_HANDLE;
        _logger.info("destroyed shader module ({})", index);
    });

    _pipelineLayoutList.clearDestroyQueue([this](i32 index, PipelineLayout& layout) {
        layout = PipelineLayout(nullptr);
        _logger.info("destroyed pipeline layout ({})", index);
    });

    for (auto it = _descriptorSets.begin(); it != _descriptorSets.end(); it++) {
        auto& frame = it.value().second;
        if (frame < 0) {
            vkFreeDescriptorSets(_context.device(), _descriptorPool, 1, &it.value().first);
            _descriptorSets.erase(it);
        } else
            --frame;
    }
    return true;
}

cala::backend::vulkan::BufferHandle cala::backend::vulkan::Device::createBuffer(u32 size, BufferUsage usage, backend::MemoryProperties flags, bool persistentlyMapped, ExtraInfo extraInfo) {
//    u32 index = 0;
    usage = usage | BufferUsage::TRANSFER_DST | BufferUsage::TRANSFER_SRC | BufferUsage::STORAGE | BufferUsage::DEVICE_ADDRESS;

    VkBuffer buffer;
    VmaAllocation allocation;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = getBufferUsage(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
//    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.requiredFlags = extraInfo.requiredFlags;
    allocInfo.preferredFlags = extraInfo.preferredFlags;

    if ((flags & MemoryProperties::DEVICE) == MemoryProperties::DEVICE) {
//        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        persistentlyMapped = false;
    }
    if ((flags & MemoryProperties::STAGING) == MemoryProperties::STAGING) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    if ((flags & MemoryProperties::READBACK) == MemoryProperties::READBACK) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    VK_TRY(vmaCreateBuffer(context().allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr));

    _totalAllocated += size;

    i32 index = _bufferList.insert(this);

    _bufferList.getResource(index)->_buffer = buffer;
    if (_context.getEnabledFeatures().deviceAddress) {
        VkBufferDeviceAddressInfo deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        deviceAddressInfo.buffer = buffer;
        _bufferList.getResource(index)->_address = vkGetBufferDeviceAddress(_context.device(), &deviceAddressInfo);
    }
    _bufferList.getResource(index)->_allocation = allocation;
    _bufferList.getResource(index)->_size = size;
    _bufferList.getResource(index)->_usage = usage;
    _bufferList.getResource(index)->_flags = flags;
    if (persistentlyMapped)
        _bufferList.getResource(index)->_mapped = _bufferList.getResource(index)->map();


    _bytesAllocatedPerFrame += size;

    updateBindlessBuffer(index);

    _context.setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)buffer, "Buffer: " + std::to_string(index));

    return _bufferList.getHandle(this, index);
}

cala::backend::vulkan::BufferHandle cala::backend::vulkan::Device::resizeBuffer(BufferHandle handle, u32 size, bool transfer) {
    assert(handle);
    auto newHandle = createBuffer(size, handle->usage(), handle->flags(), handle->persistentlyMapped());
    assert(newHandle);
    if (transfer) {
        immediate([&](backend::vulkan::CommandHandle cmd) {
            VkBufferCopy bufferCopy{};
            bufferCopy.dstOffset = 0;
            bufferCopy.srcOffset = 0;
            bufferCopy.size = handle->size();
            vkCmdCopyBuffer(cmd->buffer(), handle->buffer(), newHandle->buffer(), 1, &bufferCopy);
        });
    }
//    destroyBuffer(handle);
    return newHandle;
}

cala::backend::vulkan::ImageHandle cala::backend::vulkan::Device::createImage(Image::CreateInfo info) {
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


    _imageViews.resize(_imageList.allocated());
    _imageViews[index] = std::move(_imageList.getResource(index)->newView(0, _imageList.getResource(index)->mips()));

    assert(_imageList.allocated() == _imageViews.size());

    bool isSampled = (info.usage & ImageUsage::SAMPLED) == ImageUsage::SAMPLED;
    bool isStorage = (info.usage & ImageUsage::STORAGE) == ImageUsage::STORAGE;
    updateBindlessImage(index, _imageViews[index], isSampled, isStorage);

    _bytesAllocatedPerFrame += (info.width * info.height * info.depth * formatToSize(info.format));

    _context.setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)image, "Image: " + std::to_string(index));
    _context.setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_imageViews[index].view, "ImageView: " + std::to_string(index));

    return _imageList.getHandle(this, index);
}

cala::backend::vulkan::ImageHandle cala::backend::vulkan::Device::getImageHandle(u32 index) {
    assert(index < _imageList.allocated());
    return _imageList.getHandle(this, index);
}

cala::backend::vulkan::Image::View &cala::backend::vulkan::Device::getImageView(ImageHandle handle) {
    assert(handle);
    return _imageViews[handle.index()];
}

cala::backend::vulkan::Image::View &cala::backend::vulkan::Device::getImageView(u32 index) {
    assert(index < _imageViews.size());
    return _imageViews[index];
}

cala::backend::vulkan::ShaderModuleHandle cala::backend::vulkan::Device::createShaderModule(std::span<u32> spirv, ShaderStage stage) {
    VkShaderModule module;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(u32);
    createInfo.pCode = spirv.data();

    if (vkCreateShaderModule(context().device(), &createInfo, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Unable to create shader module");

    ShaderModuleInterface interface(spirv, stage);

    i32 index = _shaderModulesList.insert(this);
    _shaderModulesList.getResource(index)->_module = module;
    _shaderModulesList.getResource(index)->_stage = stage;
    _shaderModulesList.getResource(index)->_main = "main";
    _shaderModulesList.getResource(index)->_interface = std::move(interface);
    return _shaderModulesList.getHandle(this, index);
}

cala::backend::vulkan::ShaderModuleHandle cala::backend::vulkan::Device::recreateShaderModule(ShaderModuleHandle handle, std::span<u32> spirv, cala::backend::ShaderStage stage) {
    if (!handle)
        return createShaderModule(spirv, stage);
    i32 handleIndex = handle.index();
    VkShaderModule module;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(u32);
    createInfo.pCode = spirv.data();

    if (vkCreateShaderModule(context().device(), &createInfo, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Unable to create shader module");

    ShaderModuleInterface interface(spirv, stage);

    i32 index = _shaderModulesList.insert(this);

    _shaderModulesList._resources[index].second.swap(_shaderModulesList._resources[handleIndex].second);
    _shaderModulesList._resources[index].second->index = index;
    _shaderModulesList._resources[handleIndex].second->index = handleIndex;

    _shaderModulesList._resources[handleIndex].second->count = 0;
    _shaderModulesList._resources[handleIndex].second->deleter(handleIndex);

    _shaderModulesList.getResource(index)->_module = module;
    _shaderModulesList.getResource(index)->_stage = stage;
    _shaderModulesList.getResource(index)->_main = "main";
    _shaderModulesList.getResource(index)->_interface = std::move(interface);
    return _shaderModulesList.getHandle(this, index);
}

cala::backend::vulkan::PipelineLayoutHandle cala::backend::vulkan::Device::createPipelineLayout(const cala::backend::ShaderInterface &interface) {
    PipelineLayout layout(this, interface);

    i32 index = _pipelineLayoutList.insert(this);
    *_pipelineLayoutList._resources[index].first = std::move(layout);
    return _pipelineLayoutList.getHandle(this, index);
}


cala::backend::vulkan::SamplerHandle cala::backend::vulkan::Device::getSampler(Sampler::CreateInfo info) {
    for (i32 index = 0; index < _samplers.size(); index++) {
        if (_samplers[index].first == info)
            return { this, index, nullptr };
    }

    u32 index = _samplers.size();
    _samplers.emplace_back(std::make_pair(info, std::make_unique<Sampler>(*this, info)));
    updateBindlessSampler(index);
    return { this, static_cast<i32>(index), nullptr };
}




VkDescriptorSetLayout cala::backend::vulkan::Device::getSetLayout(std::span<VkDescriptorSetLayoutBinding> bindings) {
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

void cala::backend::vulkan::Device::updateBindlessBuffer(u32 index) {
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
    descriptorWrite.dstBinding = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(_context.device(), 1, &descriptorWrite, 0, nullptr);
}

void cala::backend::vulkan::Device::updateBindlessImage(u32 index, Image::View &image, bool sampled, bool storage) {
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
        descriptorWrite[writeNum].dstBinding = 0;
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
        descriptorWrite[writeNum].dstBinding = 3;
        descriptorWrite[writeNum].pImageInfo = &storageImageInfo;
        writeNum++;
    }

    vkUpdateDescriptorSets(_context.device(), writeNum, descriptorWrite, 0, nullptr);
}

void cala::backend::vulkan::Device::updateBindlessSampler(u32 index) {
    VkWriteDescriptorSet  descriptorWrite{};
    VkDescriptorImageInfo imageInfo{};

    imageInfo.sampler = _samplers[index].second->sampler();

    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrite.dstArrayElement = index;
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = 2;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_context.device(), 1, &descriptorWrite, 0, nullptr);
}

void cala::backend::vulkan::Device::setBindlessSetIndex(u32 index) {
//    for (auto& cmd : _frameCommands)
//        cmd.setBindlessIndex(index);
    _bindlessIndex = index;
}

cala::backend::vulkan::RenderPass* cala::backend::vulkan::Device::getRenderPass(std::span<RenderPass::Attachment> attachments) {
//    u64 hash = ende::util::murmur3(reinterpret_cast<u32*>(attachments.data()), attachments.size() * sizeof(RenderPass::Attachment), attachments.size());
    u64 hash = 0;
    for (auto& attachment : attachments) {
        hash |= (((u64)attachment.format) * 0x100000001b3ull);
        hash |= ((u64)attachment.internalLayout << 40);
        hash |= ((u64)attachment.loadOp << 35);
        hash |= ((u64)attachment.storeOp << 30);
        hash |= ((u64)attachment.finalLayout << 20);
        hash |= ((u64)attachment.initialLayout << 10);
    }

    auto it = _renderPasses.find(hash);
    if (it != _renderPasses.end())
        return it.value();

    auto a = _renderPasses.emplace(std::make_pair(hash, new RenderPass(*this, attachments)));
    return a.first.value();
}

cala::backend::vulkan::Framebuffer *cala::backend::vulkan::Device::getFramebuffer(RenderPass *renderPass, std::span<VkImageView> attachmentImages, u32 width, u32 height) {
    u64 hash = ((u64)renderPass->id() << 32);
    for (auto& attachment : attachmentImages)
        hash |= (u64)attachment;

    auto it = _framebuffers.find(hash);
    if (it != _framebuffers.end())
        return it.value();

    auto a = _framebuffers.emplace(std::make_pair(hash, new Framebuffer(_context.device(), *renderPass, attachmentImages, width, height)));
    return a.first.value();
}

void cala::backend::vulkan::Device::clearFramebuffers() {
    for (auto& framebuffer : _framebuffers)
        delete framebuffer.second;
    _framebuffers.clear();
}

VkDescriptorSet cala::backend::vulkan::Device::getDescriptorSet(CommandBuffer::DescriptorKey key) {
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

VkPipeline cala::backend::vulkan::Device::getPipeline(CommandBuffer::PipelineKey key) {
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

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = key.layout;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

//        VkPipeline pipeline;
        if (vkCreateGraphicsPipelines(context().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("Error creating pipeline");
    } else {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = key.shaders[0];
        pipelineInfo.layout = key.layout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateComputePipelines(context().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("Error creating compute pipeline");
    }

    _pipelines.emplace(std::make_pair(key, pipeline));

    return pipeline;
}

cala::backend::vulkan::Device::Stats cala::backend::vulkan::Device::stats() const {
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

void cala::backend::vulkan::Device::printMarkers() {

//    ende::fs::File file;
//    file.open("markers.log"_path, ende::fs::out);

    for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; frame++) {
//        file.write(std::format("Frame: {}", frame));
        if (!_markerBuffer[frame])
            continue;

        u32* markers = static_cast<u32*>(_markerBuffer[frame]->persistentMapping());
        for (u32 i = 0; i < _markerBuffer[frame]->size() / sizeof(u32); i++) {
            u32 marker = markers[i];
            auto cmd = marker < _markedCmds[frame].size() ? _markedCmds[i][frame] : std::make_pair( "NullCmd", 0 );
//            file.write(std::format("Command: {}\nMarker[{}]: {}\n", cmd.first, i, marker));
            _logger.warn("Command: {}\nMarker[{}]: {}", cmd.first, i, marker);
            if (marker == 0)
                break;
        }

//        file.write("\n\n\n");
    }

//    file.close();
}