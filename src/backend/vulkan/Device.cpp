#include "Cala/backend/vulkan/Device.h"
#include "Cala/backend/vulkan/primitives.h"
#include <vulkan/vulkan.h>
#include <Ende/profile/profile.h>
#include <Ende/log/log.h>

cala::backend::vulkan::Device::Device(cala::backend::Platform& platform)
    : _context(platform),
      _commandPools{
              {CommandPool(this, QueueType::GRAPHICS), CommandPool(this, QueueType::COMPUTE), CommandPool(this, QueueType::TRANSFER)},
              {CommandPool(this, QueueType::GRAPHICS), CommandPool(this, QueueType::COMPUTE), CommandPool(this, QueueType::TRANSFER)}
        },
      _frameCount(0),
      _bindlessIndex(-1),
      _defaultSampler(*this, {}),
      _defaultShadowSampler(*this, {
          .filter = VK_FILTER_NEAREST,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      })
{


    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& fence : _frameFences) {
        vkCreateFence(_context.device(), &fenceCreateInfo, nullptr, &fence);
    }

    constexpr u32 maxBindless = 1000;

    VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxBindless }
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolCreateInfo.maxSets = maxBindless;
    poolCreateInfo.poolSizeCount = 1;
    poolCreateInfo.pPoolSizes = poolSizes;
    VK_TRY(vkCreateDescriptorPool(_context.device(), &poolCreateInfo, nullptr, &_bindlessPool));

    VkDescriptorSetLayoutBinding bindlessLayoutBinding{};
    bindlessLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindlessLayoutBinding.descriptorCount = maxBindless;
    bindlessLayoutBinding.binding = 0;
    bindlessLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
    bindlessLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo bindlessLayoutCreateInfo{};
    bindlessLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    bindlessLayoutCreateInfo.bindingCount = 1;
    bindlessLayoutCreateInfo.pBindings = &bindlessLayoutBinding;
    bindlessLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo;
    extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    extendedInfo.bindingCount = 1;
    extendedInfo.pBindingFlags = &bindingFlags;
    bindlessLayoutCreateInfo.pNext = &extendedInfo;

    VK_TRY(vkCreateDescriptorSetLayout(_context.device(), &bindlessLayoutCreateInfo, nullptr, &_bindlessLayout));

    VkDescriptorSetAllocateInfo bindlessAllocate{};
    bindlessAllocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    bindlessAllocate.descriptorSetCount = 1;
    bindlessAllocate.pSetLayouts = &_bindlessLayout;
    bindlessAllocate.descriptorPool = _bindlessPool;

    VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{};
    countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    countInfo.descriptorSetCount = 1;
    u32 maxBinding = maxBindless - 1;
    countInfo.pDescriptorCounts = &maxBinding;
    bindlessAllocate.pNext = &countInfo;

    VK_TRY(vkAllocateDescriptorSets(_context.device(), &bindlessAllocate, &_bindlessSet));

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
}

cala::backend::vulkan::Device::~Device() {
    VK_TRY(vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS))); //ensures last frame finished before destroying stuff

    for (auto& buffer : _buffers) {
        buffer._mapped = Buffer::Mapped();
        VkBuffer buf = buffer._buffer;
        VmaAllocation allocation = buffer._allocation;
        if (allocation)
            vmaDestroyBuffer(context().allocator(), buf, allocation);
        buffer._allocation = nullptr;
    }

    _imageViews.clear();
    for (auto& image : _images) {
        VkImage im = image._image;
        VmaAllocation allocation = image._allocation;
        if (im != VK_NULL_HANDLE)
            vmaDestroyImage(context().allocator(), im, allocation);
        image._allocation = nullptr;
    }

    for (auto* program : _programs)
        delete program;

    clearFramebuffers();

    for (auto& renderPass : _renderPasses)
        delete renderPass.second;

    for (auto& pipeline : _pipelines)
        vkDestroyPipeline(_context.device(), pipeline.second, nullptr);

    vkDestroyDescriptorSetLayout(_context.device(), _bindlessLayout, nullptr);

    vkDestroyDescriptorPool(_context.device(), _descriptorPool, nullptr);
    vkDestroyDescriptorPool(_context.device(), _bindlessPool, nullptr);

    for (auto& fence : _frameFences) {
        vkDestroyFence(_context.device(), fence, nullptr);
    }

    for (auto& setLayout : _setLayouts)
        vkDestroyDescriptorSetLayout(_context.device(), setLayout.second, nullptr);
}


cala::backend::vulkan::Device::FrameInfo cala::backend::vulkan::Device::beginFrame() {
    _frameCount++;

    VkFence fence = _frameFences[frameIndex()];

    waitFrame(_frameCount);

    vmaSetCurrentFrameIndex(_context.allocator(), _frameCount);
    _commandPools[0][frameIndex()].reset();

    return {
        _frameCount,
        &getCommandBuffer(frameIndex()),
        fence
    };
}

ende::time::Duration cala::backend::vulkan::Device::endFrame() {
    _lastFrameTime = _frameClock.reset();
    return _lastFrameTime;
}

bool cala::backend::vulkan::Device::waitFrame(u64 frame, u64 timeout) {
    VkFence fence = _frameFences[frame % FRAMES_IN_FLIGHT];

    auto res = vkWaitForFences(_context.device(), 1, &fence, true, timeout) == VK_SUCCESS;
    if (res)
        vkResetFences(_context.device(), 1, &fence);
    return res;
}

bool cala::backend::vulkan::Device::wait(u64 timeout) {
    bool res = false;
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        res = waitFrame(i);
    return res;
}

cala::backend::vulkan::BufferHandle cala::backend::vulkan::Device::stagingBuffer(u32 size) {
    return createBuffer(size, BufferUsage::TRANSFER_SRC, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_CACHED | MemoryProperties::HOST_COHERENT);
}


cala::backend::vulkan::CommandBuffer& cala::backend::vulkan::Device::beginSingleTimeCommands(QueueType queueType) {

    auto& buffer = getCommandBuffer(frameIndex(), queueType);
    buffer.begin();
    return buffer;
}

void cala::backend::vulkan::Device::endSingleTimeCommands(CommandBuffer& buffer) {
    VkCommandBuffer buf = buffer.buffer();
    VkFence fence; //TODO: dont create/destroy fence each time
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    VK_TRY(vkCreateFence(context().device(), &fenceCreateInfo, nullptr, &fence));
    buffer.submit(nullptr, fence);

    auto res = vkWaitForFences(context().device(), 1, &fence, true, 1000000000) == VK_SUCCESS;
    if (res)
        vkResetFences(context().device(), 1, &fence);
    else
        ende::log::error("Failed waiting for immediate fence");
    vkDestroyFence(context().device(), fence, nullptr);
}

cala::backend::vulkan::CommandBuffer& cala::backend::vulkan::Device::getCommandBuffer(u32 frame, QueueType queueType) {
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
    }
    return _commandPools[frame][index].getBuffer();
}


bool cala::backend::vulkan::Device::gc() {
    for (auto it = _buffersToDestroy.begin(); it != _buffersToDestroy.end(); it++) {
        auto& frame = it->first;
        auto& handle = it->second;
        if (frame <= 0) {
            u32 index = handle.index();
            _buffers[index]._mapped = Buffer::Mapped();
            VkBuffer buffer = _buffers[index]._buffer;
            VmaAllocation allocation = _buffers[index]._allocation;
            if (allocation)
                vmaDestroyBuffer(context().allocator(), buffer, allocation);
            _buffers[index]._allocation = nullptr;
            _buffers[index] = Buffer(this);
            _freeBuffers.push(index);
            _buffersToDestroy.erase(it--);
            ende::log::info("destroyed buffer ({})", index);
        } else
            --frame;
    }

    for (auto it = _imagesToDestroy.begin(); it != _imagesToDestroy.end(); it++) {
        auto& frame = it->first;
        auto& handle = it->second;
        if (frame <= 0) {
            u32 index = handle.index();
            VkImage image = _images[index]._image;
            VmaAllocation allocation = _images[index]._allocation;
            _imageViews[index] = backend::vulkan::Image::View();
            updateBindlessImage(index, _imageViews[0], _defaultSampler);
            if (image != VK_NULL_HANDLE)
                vmaDestroyImage(context().allocator(), image, allocation);
            _images[index]._allocation = nullptr;
            _images[index] = Image(this);
            _freeImages.push(index);
            _imagesToDestroy.erase(it--);
            ende::log::info("destroyed image ({})", index);
        } else
            --frame;
    }
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

cala::backend::vulkan::BufferHandle cala::backend::vulkan::Device::createBuffer(u32 size, BufferUsage usage, backend::MemoryProperties flags, bool persistentlyMapped) {
    u32 index = 0;
    usage = usage | BufferUsage::TRANSFER_DST | BufferUsage::TRANSFER_SRC;

    VkBuffer buffer;
    VmaAllocation allocation;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = getBufferUsage(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if ((flags & MemoryProperties::HOST_VISIBLE) == MemoryProperties::HOST_VISIBLE) {
        if ((flags & MemoryProperties::HOST_CACHED) == MemoryProperties::HOST_CACHED)
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        else
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VK_TRY(vmaCreateBuffer(context().allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr));
    if (!_freeBuffers.empty()) {
        index = _freeBuffers.pop().value();
        _buffers[index] = backend::vulkan::Buffer(this);
    } else {
        index = _buffers.size();
        _buffers.emplace(backend::vulkan::Buffer(this));
    }
    _buffers[index]._buffer = buffer;
    _buffers[index]._allocation = allocation;
    _buffers[index]._size = size;
    _buffers[index]._usage = usage;
    _buffers[index]._flags = flags;
    if (persistentlyMapped)
        _buffers[index]._mapped = _buffers[index].map();

    return { this, static_cast<i32>(index) };
}

void cala::backend::vulkan::Device::destroyBuffer(BufferHandle handle) {
    _buffersToDestroy.push(std::make_pair(FRAMES_IN_FLIGHT + 1, handle));
}

cala::backend::vulkan::BufferHandle cala::backend::vulkan::Device::resizeBuffer(BufferHandle handle, u32 size, bool transfer) {
    assert(handle);
    auto newHandle = createBuffer(size, handle->usage(), handle->flags(), handle->persistentlyMapped());
    assert(newHandle);
    if (transfer) {
        immediate([&](backend::vulkan::CommandBuffer& cmd) {
            VkBufferCopy bufferCopy{};
            bufferCopy.dstOffset = 0;
            bufferCopy.srcOffset = 0;
            bufferCopy.size = handle->size();
            vkCmdCopyBuffer(cmd.buffer(), handle->buffer(), newHandle->buffer(), 1, &bufferCopy);
        });
    }
    destroyBuffer(handle);
    return newHandle;
}

cala::backend::vulkan::ImageHandle cala::backend::vulkan::Device::createImage(Image::CreateInfo info, Sampler *sampler) {
    u32 index = 0;

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

    if (!_freeImages.empty()) {
        index = _freeImages.pop().value();
        _images[index] = backend::vulkan::Image(this);
    } else {
        index = _images.size();
        _images.emplace(backend::vulkan::Image(this));
    }

    _images[index]._image = image;
    _images[index]._allocation = allocation;
    _images[index]._width = info.width;
    _images[index]._height = info.height;
    _images[index]._depth = info.depth;
    _images[index]._layers = info.arrayLayers;
    _images[index]._mips = info.mipLevels;
    _images[index]._format = info.format;
    _images[index]._usage = info.usage;
    _images[index]._type = type;


    _imageViews.resize(_images.size());
    _imageViews[index] = std::move(_images[index].newView(0, _images[index].mips()));

    assert(_images.size() == _imageViews.size());

    backend::vulkan::Sampler* chosenSampler = sampler;
    if (!chosenSampler)
        chosenSampler = backend::isDepthFormat(info.format) ? &_defaultShadowSampler : &_defaultSampler;

    updateBindlessImage(index, _imageViews[index], *chosenSampler);
    return { this, static_cast<i32>(index) };
}

void cala::backend::vulkan::Device::destroyImage(ImageHandle handle) {
    _imagesToDestroy.push(std::make_pair(FRAMES_IN_FLIGHT + 1, handle));
}

cala::backend::vulkan::ImageHandle cala::backend::vulkan::Device::getImageHandle(u32 index) {
    assert(index < _images.size());
    return { this, static_cast<i32>(index) };
}

cala::backend::vulkan::Image::View &cala::backend::vulkan::Device::getImageView(ImageHandle handle) {
    assert(handle);
    return _imageViews[handle.index()];
}

cala::backend::vulkan::Image::View &cala::backend::vulkan::Device::getImageView(u32 index) {
    assert(index < _images.size());
    return _imageViews[index];
}

cala::backend::vulkan::ProgramHandle cala::backend::vulkan::Device::createProgram(ShaderProgram &&program) {
    u32 index = _programs.size();
    _programs.push(new ShaderProgram(std::move(program)));
    return { this, static_cast<i32>(index) };
}





VkDescriptorSetLayout cala::backend::vulkan::Device::getSetLayout(ende::Span <VkDescriptorSetLayoutBinding> bindings) {
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

void cala::backend::vulkan::Device::updateBindlessImage(u32 index, Image::View &image, Sampler& sampler) {
    VkWriteDescriptorSet descriptorWrite{};
    VkDescriptorImageInfo imageInfo{};

    imageInfo.imageView = image.view;
    imageInfo.sampler = sampler.sampler();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.dstArrayElement = index;
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_context.device(), 1, &descriptorWrite, 0, nullptr);
}

void cala::backend::vulkan::Device::setBindlessSetIndex(u32 index) {
//    for (auto& cmd : _frameCommands)
//        cmd.setBindlessIndex(index);
    _bindlessIndex = index;
}

cala::backend::vulkan::RenderPass* cala::backend::vulkan::Device::getRenderPass(ende::Span<RenderPass::Attachment> attachments) {
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

cala::backend::vulkan::Framebuffer *cala::backend::vulkan::Device::getFramebuffer(RenderPass *renderPass, ende::Span<VkImageView> attachmentImages, ende::Span<u32> attachmentHashes, u32 width, u32 height) {
    u64 hash = ((u64)renderPass->id() << 32);
    for (auto& attachment : attachmentHashes)
        hash |= attachment;

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
    u32 allocatedBuffers = _buffers.size();
    u32 buffersInUse = allocatedBuffers - _freeBuffers.size();
    u32 allocatedImages = _images.size();
    u32 imagesInUse = allocatedImages - _freeImages.size();
    u32 descriptorSetCount = _descriptorSets.size();
    u32 pipelineCount = _pipelines.size();
    return {
        buffersInUse,
        allocatedBuffers,
        imagesInUse,
        allocatedImages,
        descriptorSetCount,
        pipelineCount
    };
}