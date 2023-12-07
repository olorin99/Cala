#include <Cala/vulkan/CommandPool.h>
#include <Cala/vulkan/Device.h>

cala::vk::CommandPool::CommandPool(Device *device, QueueType queueType)
    : _device(device),
    _pool(VK_NULL_HANDLE),
    _queueType(queueType),
    _index(0)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    u32 index = 0;
    _device->context().queueIndex(index, queueType);
    createInfo.queueFamilyIndex = index;
    VK_TRY(vkCreateCommandPool(_device->context().device(), &createInfo, nullptr, &_pool));
}

cala::vk::CommandPool::~CommandPool() {
    destroy();
//    for (auto& buffer : _buffers) {
//        auto buf = buffer.buffer();
//        vkFreeCommandBuffers(_device->context().device(), _pool, 1, &buf);
//    }
//    vkDestroyCommandPool(_device->context().device(), _pool, nullptr);
}

cala::vk::CommandPool::CommandPool(cala::vk::CommandPool &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_pool, rhs._pool);
    std::swap(_buffers, rhs._buffers);
    std::swap(_queueType, rhs._queueType);
    std::swap(_index, rhs._index);
}

cala::vk::CommandPool &cala::vk::CommandPool::operator=(cala::vk::CommandPool &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_pool, rhs._pool);
    std::swap(_buffers, rhs._buffers);
    std::swap(_queueType, rhs._queueType);
    std::swap(_index, rhs._index);
    return *this;
}

void cala::vk::CommandPool::reset() {
    VK_TRY(vkResetCommandPool(_device->context().device(), _pool, 0));
    _index = 0;
}

cala::vk::CommandHandle cala::vk::CommandPool::getBuffer() {
    if (_index < _buffers.size())
        return { this, static_cast<i32>(_index++), nullptr };

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer buffer;
    VK_TRY(vkAllocateCommandBuffers(_device->context().device(), &allocInfo, &buffer));
    _buffers.emplace_back(*_device, _device->context().getQueue(_queueType), buffer);
    return { this, static_cast<i32>(_index++), nullptr };
}

void cala::vk::CommandPool::destroy() {
    if (_pool == VK_NULL_HANDLE)
        return;
    for (auto& buffer : _buffers) {
        auto buf = buffer.buffer();
        vkFreeCommandBuffers(_device->context().device(), _pool, 1, &buf);
    }
    vkDestroyCommandPool(_device->context().device(), _pool, nullptr);
    _buffers.clear();
    _pool = VK_NULL_HANDLE;
}