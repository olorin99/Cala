#include "Cala/backend/vulkan/Driver.h"
#include <vulkan/vulkan.h>

cala::backend::vulkan::Driver::Driver(cala::backend::Platform& platform)
    : _context(platform),
      _swapchain(*this, platform),
      _commands(_context, _context.queueIndex(VK_QUEUE_GRAPHICS_BIT)),
      _commandPool(VK_NULL_HANDLE)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = _context.queueIndex(VK_QUEUE_GRAPHICS_BIT);
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(_context.device(), &createInfo, nullptr, &_commandPool);
}

cala::backend::vulkan::Driver::~Driver() {
    vkDestroyCommandPool(_context.device(), _commandPool, nullptr);

    for (auto& setLayout : _setLayouts)
        vkDestroyDescriptorSetLayout(_context.device(), setLayout.second, nullptr);
}


cala::backend::vulkan::CommandBuffer* cala::backend::vulkan::Driver::beginFrame() {
    return _commands.get();
}

ende::time::Duration cala::backend::vulkan::Driver::endFrame() {
    _commands.flush();
    _lastFrameTime = _frameClock.reset();
    return _lastFrameTime;
}


void cala::backend::vulkan::Driver::draw(CommandBuffer::RasterState state, Primitive primitive) {

    auto commandBuffer = _commands.get();

//    _context._pipelines->bindRasterState(state);
//    _context._pipelines->bindPipeline(commandBuffer);

    commandBuffer->bindRasterState(state);
    commandBuffer->bindPipeline();

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->buffer(), 0, 1, &primitive.vertex, offsets);

    vkCmdDraw(commandBuffer->buffer(), primitive.vertices, 1, 0, 0);

}

cala::backend::vulkan::Buffer cala::backend::vulkan::Driver::stagingBuffer(u32 size) {
    return Buffer(*this, size, BufferUsage::TRANSFER_SRC, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
}


VkCommandBuffer cala::backend::vulkan::Driver::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_context.device(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void cala::backend::vulkan::Driver::endSingleTimeCommands(VkCommandBuffer buffer) {
    vkEndCommandBuffer(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    VkQueue graphicsQueue = _context.getQueue(VK_QUEUE_GRAPHICS_BIT);
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(_context.device(), _commandPool, 1, &buffer);
}

VkDeviceMemory cala::backend::vulkan::Driver::allocate(u32 size, u32 typeBits, MemoryProperties flags) {
    return _context.allocate(size, typeBits, flags);
}






VkDescriptorSetLayout cala::backend::vulkan::Driver::getSetLayout(ende::Span <VkDescriptorSetLayoutBinding> bindings) {
    SetLayoutKey key;
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