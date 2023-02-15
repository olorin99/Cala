#include "Cala/backend/vulkan/Device.h"
#include <vulkan/vulkan.h>

#include <Ende/log/log.h>

cala::backend::vulkan::Device::Device(cala::backend::Platform& platform, bool clear)
    : _context(platform),
      _swapchain(nullptr),
      _commandPool(VK_NULL_HANDLE),
      _frameCommands{
          CommandBuffer(*this, _context.getQueue(QueueType::GRAPHICS), VK_NULL_HANDLE),
          CommandBuffer(*this, _context.getQueue(QueueType::GRAPHICS), VK_NULL_HANDLE)
      },
      _frameCount(0),
      _bindlessIndex(-1)
{
    _swapchain = new Swapchain(*this, platform, clear);
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = _context.queueIndex(QueueType::GRAPHICS);
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(_context.device(), &createInfo, nullptr, &_commandPool);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = FRAMES_IN_FLIGHT;

    VkCommandBuffer buffers[FRAMES_IN_FLIGHT];

    vkAllocateCommandBuffers(_context.device(), &allocInfo, buffers);

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        _frameCommands[i].setBuffer(buffers[i]);
        vkCreateFence(_context.device(), &fenceCreateInfo, nullptr, &_frameFences[i]);
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
    vkCreateDescriptorPool(_context.device(), &poolCreateInfo, nullptr, &_bindlessPool);

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

    vkCreateDescriptorSetLayout(_context.device(), &bindlessLayoutCreateInfo, nullptr, &_bindlessLayout);

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

    vkAllocateDescriptorSets(_context.device(), &bindlessAllocate, &_bindlessSet);
}

cala::backend::vulkan::Device::~Device() {
    vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS)); //ensures last frame finished before destroying stuff

    clearFramebuffers();

    for (auto& renderPass : _renderPasses)
        delete renderPass.second;

    vkDestroyDescriptorSetLayout(_context.device(), _bindlessLayout, nullptr);

    vkDestroyDescriptorPool(_context.device(), _bindlessPool, nullptr);

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(_context.device(), _frameFences[i], nullptr);
    }

    vkDestroyCommandPool(_context.device(), _commandPool, nullptr);

    for (auto& setLayout : _setLayouts)
        vkDestroyDescriptorSetLayout(_context.device(), setLayout.second, nullptr);
    delete _swapchain;
}


cala::backend::vulkan::Device::FrameInfo cala::backend::vulkan::Device::beginFrame() {

    CommandBuffer* cmd = &_frameCommands[_frameCount % FRAMES_IN_FLIGHT];
    VkFence fence = _frameFences[_frameCount % FRAMES_IN_FLIGHT];

    vmaSetCurrentFrameIndex(_context.allocator(), _frameCount);

    return {
        _frameCount++,
        cmd,
        fence,
        _swapchain->nextImage()
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
        vkResetFences(_context.device(), 1, &_frameFences[frame % FRAMES_IN_FLIGHT]);
    return res;
}

bool cala::backend::vulkan::Device::wait(u64 timeout) {
    bool res = false;
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        res = waitFrame(i);
    return res;
}

cala::backend::vulkan::Buffer cala::backend::vulkan::Device::stagingBuffer(u32 size) {
    return {*this, size, BufferUsage::TRANSFER_SRC, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT};
}


cala::backend::vulkan::CommandBuffer cala::backend::vulkan::Device::beginSingleTimeCommands(QueueType queueType) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_context.device(), &allocInfo, &commandBuffer);
    VkQueue queue = _context.getQueue(queueType);
    CommandBuffer buffer(*this, queue, commandBuffer);

    buffer.begin();
    return buffer;
}

void cala::backend::vulkan::Device::endSingleTimeCommands(CommandBuffer& buffer) {
    VkCommandBuffer buf = buffer.buffer();
    VkFence fence; //TODO: dont create/destroy fence each time
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    vkCreateFence(context().device(), &fenceCreateInfo, nullptr, &fence);
    buffer.submit(nullptr, fence);

    auto res = vkWaitForFences(context().device(), 1, &fence, true, 1000000000) == VK_SUCCESS;
    if (res)
        vkResetFences(context().device(), 1, &fence);
    else
        ende::log::error("Failed waiting for immediate fence");
    vkDestroyFence(context().device(), fence, nullptr);

    vkFreeCommandBuffers(_context.device(), _commandPool, 1, &buf);
}

VkDeviceMemory cala::backend::vulkan::Device::allocate(u32 size, u32 typeBits, MemoryProperties flags) {
    return _context.allocate(size, typeBits, flags);
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
    vkCreateDescriptorSetLayout(_context.device(), &createInfo, nullptr, &setLayout);

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
    for (auto& cmd : _frameCommands)
        cmd.setBindlessIndex(index);
    _bindlessIndex = index;
}

cala::backend::vulkan::RenderPass* cala::backend::vulkan::Device::getRenderPass(ende::Span<RenderPass::Attachment> attachments) {
//    u64 hash = ende::util::murmur3(reinterpret_cast<u32*>(attachments.data()), attachments.size() * sizeof(RenderPass::Attachment), attachments.size());
    u64 hash = 0;
    for (auto& attachment : attachments) {
        hash |= (((u64)attachment.format) * 0x100000001b3ull);
//        ende::log::info("format:{}", (u32)attachment.format);
        hash |= ((u64)attachment.internalLayout << 40);
        hash |= ((u64)attachment.loadOp << 35);
        hash |= ((u64)attachment.storeOp << 30);
        hash |= ((u64)attachment.finalLayout << 20);
        hash |= ((u64)attachment.initialLayout << 10);
    }
//    ende::log::info("end hash: {}", hash);

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