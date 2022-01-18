#include "Cala/backend/vulkan/CommandBufferList.h"

cala::backend::vulkan::CommandBufferList::CommandBufferList(Context& context, u32 queueIndex)
    : _context(context),
    _pool(VK_NULL_HANDLE),
    _queue(VK_NULL_HANDLE),
    _current(nullptr)
{

    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = queueIndex;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(_context._device, &createInfo, nullptr, &_pool);

    vkGetDeviceQueue(_context._device, queueIndex, 0, &_queue);

}

cala::backend::vulkan::CommandBufferList::~CommandBufferList() {
    vkDestroyCommandPool(_context._device, _pool, nullptr);
}


cala::backend::vulkan::CommandBuffer* cala::backend::vulkan::CommandBufferList::get() {
    // if command buffer already started return current
    if (_current)
        return _current;

    // find next command buffer that is available
//    for (auto& buffer : _buffers) {
//        if (buffer.free) {
//            _current = &buffer;
//            break;
//        }
//    }

    for (auto& buffer : _buffers) {
        if (!buffer.active()) {
            _current = &buffer;
            break;
        }
    }


    // if non available create new
    if (!_current) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer buffer;
        vkAllocateCommandBuffers(_context._device, &allocInfo, &buffer);

        _buffers.emplace(_context._device, _queue, buffer);
        _current = &_buffers.back();
    }

    _current->begin();
    return _current;
}

bool cala::backend::vulkan::CommandBufferList::flush() {
    if (!_current)
        return false;

    if (_current->active())
        _current->end();

    _current = nullptr;
    return true;
}