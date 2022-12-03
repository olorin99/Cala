#include "Cala/backend/vulkan/Driver.h"
#include <vulkan/vulkan.h>



cala::backend::vulkan::Driver::Driver(cala::backend::Platform& platform)
    : _context(platform),
      _swapchain(*this, platform),
      _commandPool(VK_NULL_HANDLE),
      _frameCommands{
          CommandBuffer(*this, _context.getQueue(QueueType::GRAPHICS), VK_NULL_HANDLE),
          CommandBuffer(*this, _context.getQueue(QueueType::GRAPHICS), VK_NULL_HANDLE)
      },
      _frameCount(0)
{
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

}

cala::backend::vulkan::Driver::~Driver() {
//    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
//        vkWaitForFences(_context.device(), 1, &_frameFences[i], true, 1000000000);
//        vkResetCommandBuffer(_frameCommands[i].buffer(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
//    }
    vkQueueWaitIdle(_context.getQueue(QueueType::GRAPHICS)); //ensures last frame finished before destroying stuff

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(_context.device(), _frameFences[i], nullptr);
    }

    vkDestroyCommandPool(_context.device(), _commandPool, nullptr);

    for (auto& setLayout : _setLayouts)
        vkDestroyDescriptorSetLayout(_context.device(), setLayout.second, nullptr);
}


cala::backend::vulkan::Driver::FrameInfo cala::backend::vulkan::Driver::beginFrame() {

    CommandBuffer* cmd = &_frameCommands[_frameCount % FRAMES_IN_FLIGHT];
    VkFence fence = _frameFences[_frameCount % FRAMES_IN_FLIGHT];

    return {
        _frameCount,
        cmd,
        fence
    };
}

ende::time::Duration cala::backend::vulkan::Driver::endFrame() {
    _lastFrameTime = _frameClock.reset();
    return _lastFrameTime;
}

bool cala::backend::vulkan::Driver::waitFrame(u64 frame, u64 timeout) {
    VkFence fence = _frameFences[frame % FRAMES_IN_FLIGHT];

    auto res = vkWaitForFences(_context.device(), 1, &fence, true, timeout) == VK_SUCCESS;
    if (res)
        vkResetFences(_context.device(), 1, &_frameFences[frame % FRAMES_IN_FLIGHT]);
    return res;
}

bool cala::backend::vulkan::Driver::wait(u64 timeout) {
    bool res = false;
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        res = waitFrame(i);
    return res;
}


void cala::backend::vulkan::Driver::draw(CommandBuffer::RasterState state, Primitive primitive) {


//    _context._pipelines->bindRasterState(state);
//    _context._pipelines->bindPipeline(commandBuffer);

//    commandBuffer->bindRasterState(state);
//    commandBuffer->bindPipeline();
//
//    VkDeviceSize offsets[] = {0};
//    vkCmdBindVertexBuffers(commandBuffer->buffer(), 0, 1, &primitive.vertex, offsets);
//
//    vkCmdDraw(commandBuffer->buffer(), primitive.vertices, 1, 0, 0);

}

cala::backend::vulkan::Buffer cala::backend::vulkan::Driver::stagingBuffer(u32 size) {
    return {*this, size, BufferUsage::TRANSFER_SRC, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT};
}


cala::backend::vulkan::CommandBuffer cala::backend::vulkan::Driver::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_context.device(), &allocInfo, &commandBuffer);
    VkQueue graphicsQueue = _context.getQueue(QueueType::GRAPHICS);
    CommandBuffer buffer(*this, graphicsQueue, commandBuffer);

    buffer.begin();
    return buffer;
}

void cala::backend::vulkan::Driver::endSingleTimeCommands(CommandBuffer& buffer) {
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
    vkDestroyFence(context().device(), fence, nullptr);

    vkFreeCommandBuffers(_context.device(), _commandPool, 1, &buf);
}

VkDeviceMemory cala::backend::vulkan::Driver::allocate(u32 size, u32 typeBits, MemoryProperties flags) {
    return _context.allocate(size, typeBits, flags);
}






VkDescriptorSetLayout cala::backend::vulkan::Driver::getSetLayout(ende::Span <VkDescriptorSetLayoutBinding> bindings) {
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