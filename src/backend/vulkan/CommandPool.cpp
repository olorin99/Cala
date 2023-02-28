#include "Cala/backend/vulkan/CommandPool.h"
#include <Cala/backend/vulkan/Device.h>

cala::backend::vulkan::CommandPool::CommandPool(Device *device, QueueType queueType)
    : _device(device),
    _pool(VK_NULL_HANDLE),
    _queueType(queueType),
    _index(0)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = _device->context().queueIndex(QueueType::GRAPHICS);
    vkCreateCommandPool(_device->context().device(), &createInfo, nullptr, &_pool);
}

cala::backend::vulkan::CommandPool::~CommandPool() {
    for (auto& buffer : _buffers) {
        auto buf = buffer.buffer();
        vkFreeCommandBuffers(_device->context().device(), _pool, 1, &buf);
    }
    vkDestroyCommandPool(_device->context().device(), _pool, nullptr);
}

void cala::backend::vulkan::CommandPool::reset() {
    vkResetCommandPool(_device->context().device(), _pool, 0);
    _index = 0;
}

cala::backend::vulkan::CommandBuffer &cala::backend::vulkan::CommandPool::getBuffer() {
    if (_index < _buffers.size())
        return _buffers[_index];

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer buffer;
    vkAllocateCommandBuffers(_device->context().device(), &allocInfo, &buffer);
    _buffers.emplace(*_device, _device->context().getQueue(_queueType), buffer);
    ++_index;
    return _buffers.back();
}